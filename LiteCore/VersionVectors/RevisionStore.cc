//
//  RevisionStore.cc
//  Couchbase Lite Core
//
//  Created by Jens Alfke on 7/8/16.
//  Copyright (c) 2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "RevisionStore.hh"
#include "Revision.hh"
#include "Error.hh"
#include "RecordEnumerator.hh"
#include "varint.hh"

/** IMPLEMENTATION NOTES:
 
    RevisionStore uses two KeyStores:

    `_currentStore` (the database's default KeyStore) stores only current revisions.
    The key is the exact record ID; meta contains the flags, version vector and record type;
    and the body is the record body (JSON or Fleece; LiteCore currently doesn't care.)
 
    `nonCurrentStore` (named "revs") stores non-current revisions. These are usually conflicts,
    but if using CAS this also contains the server ancestor of the current revision.
    The key is the docID plus revID; meta and body are the same as in `_currentStore`.
 */

namespace litecore {


    // Separates the docID and the author in the keys of non-current Revisions.
    static const char kDocIDDelimiter ='\t';

    // Separates the author and generation in the keys of non-current Revisions.
    static const char kAuthorDelimiter = ',';


    RevisionStore::RevisionStore(DataFile *db, peerID myPeerID)
    :_currentStore(db->defaultKeyStore()),
     _nonCurrentStore(db->getKeyStore("revs")),
     _myPeerID(myPeerID)
    { }


#pragma mark - GET:


    // Get the current revision of a record
    Revision::Ref RevisionStore::get(slice docID, ContentOptions opt) const {
        Record rec(docID);
        if (!_currentStore.read(rec, opt))
            return nullptr;
        return std::make_unique<Revision>(std::move(rec));
    }


    Revision::Ref RevisionStore::get(slice docID, slice revID, ContentOptions opt) const {
        // No revID means current revision:
        if (revID.size == 0)
            return get(docID, opt);

        // Look in non-current revision store:
        auto rev = getNonCurrent(docID, revID, opt);
        if (rev)
            return rev;

        // Not found; see if it's the current revision:
        rev = get(docID, opt);
        if (rev && rev->revID() == revID)
            return rev;
        return nullptr;
    }


    // Get a revision from the _nonCurrentStore only
    Revision::Ref RevisionStore::getNonCurrent(slice docID, slice revID, ContentOptions opt) const {
        Assert(revID.size > 0);
        Record rec(keyForNonCurrentRevision(docID, Version{revID}));
        if (!_nonCurrentStore.read(rec, opt))
            return nullptr;
        return std::make_unique<Revision>(std::move(rec));
    }


    // Make sure a Revision has a body (if it was originally loaded as meta-only)
    void RevisionStore::readBody(litecore::Revision &rev) {
        KeyStore &store = rev.isCurrent() ? _currentStore : _nonCurrentStore;
        store.readBody(rev.record());
    }


    // How does this revision compare to what's in the database?
    versionOrder RevisionStore::checkRevision(slice docID, slice revID) {
        Assert(revID.size);
        Version checkVers(revID);
        auto rev = get(docID);
        if (rev) {
            auto order = checkVers.compareTo(rev->version());
            if (order != kOlder)
                return order;    // Current revision is equal or newer
            if (rev->isConflicted()) {
                auto e = enumerateRevisions(docID);
                while (e.next()) {
                    Revision conflict(e.record());
                    order = checkVers.compareTo(conflict.version());
                    if (order != kOlder)
                        return order;
                }
            }
        }
        return kOlder;
    }


#pragma mark - PUT:


    // Creates a new revision.
    Revision::Ref RevisionStore::create(slice docID,
                                        const VersionVector &parentVersion,
                                        Revision::BodyParams body,
                                        Transaction &t)
    {
        // Check for conflict, and compute new version-vector:
        auto current = get(docID, kMetaOnly);
        VersionVector newVersion;
        if (current)
            newVersion = current->version();
        if (parentVersion != newVersion)
            return nullptr;
        newVersion.incrementGen(kMePeerID);

        auto newRev = std::make_unique<Revision>(docID, newVersion, body, true);
        replaceCurrent(*newRev, current.get(), t);
        return newRev;
    }


    // Inserts a revision, probably from a peer.
    versionOrder RevisionStore::insert(Revision &newRev, Transaction &t) {
        auto current = get(newRev.docID(), kMetaOnly);
        auto cmp = current ? newRev.version().compareTo(current->version()) : kNewer;
        switch (cmp) {
            case kSame:
            case kOlder:
                // This revision already exists, or is obsolete: no-op
                break;

            case kNewer:
                // This revision is newer than the current one, so replace it:
                replaceCurrent(newRev, current.get(), t);
                break;

            case kConflicting:
                // Oops, it conflicts. Delete any saved revs that are ancestors of it,
                // then save it to the non-current store and mark the current rev as conflicted:
                deleteAncestors(newRev, t);
                newRev.setCurrent(false);
                newRev.setConflicted(true);
                _nonCurrentStore.write(newRev.record(), t);
                markConflicted(*current, true, t);
                break;
        }
        return cmp;
    }


    // Creates a new revision that resolves a conflict.
    Revision::Ref RevisionStore::resolveConflict(const std::vector<Revision*> &conflicting,
                                                 Revision::BodyParams body,
                                                 Transaction &t)
    {
        return resolveConflict(conflicting, nullslice, body, t);
        // CASRevisionStore overrides this
    }

    Revision::Ref RevisionStore::resolveConflict(const std::vector<Revision*> &conflicting,
                                                 slice keepRevID,
                                                 Revision::BodyParams bodyParams,
                                                 Transaction &t)
    {
        Assert(conflicting.size() >= 2);
        VersionVector newVersion;
        Revision* current = nullptr;
        for (auto rev : conflicting) {
            newVersion = newVersion.mergedWith(rev->version());
            if (rev->isCurrent())
                current = rev;
            else if (rev->revID() != keepRevID)
                _nonCurrentStore.del(rev->record(), t);
        }
        if (!current)
            error::_throw(error::InvalidParameter); // Merge must include current revision
        newVersion.insertMergeRevID(_myPeerID, bodyParams.body);

        slice docID = conflicting[0]->docID();
        bodyParams.conflicted = hasConflictingRevisions(docID);
        auto newRev = std::make_unique<Revision>(docID, newVersion, bodyParams, true);
        _currentStore.write(newRev->record(), t);
        return newRev;
    }


    void RevisionStore::markConflicted(Revision &current, bool conflicted, Transaction &t) {
        if (current.setConflicted(conflicted)) {
            _currentStore.readBody(current.record());
            _currentStore.write(current.record(), t);
            //OPT: This is an expensive way to set a single flag, and it bumps the sequence too
        }
    }


    void RevisionStore::purge(slice docID, Transaction &t) {
        if (_currentStore.del(docID, t)) {
            RecordEnumerator e = enumerateRevisions(docID);
            while (e.next())
                _nonCurrentStore.del(e.record(), t);
        }
    }


    // Replace the current revision `current` with `newRev`
    void RevisionStore::replaceCurrent(Revision &newRev, Revision *current, Transaction &t) {
        if (current) {
            willReplaceCurrentRevision(*current, newRev, t);
            if (current->isConflicted())
                deleteAncestors(newRev, t);
        }
        newRev.setCurrent(true);    // update key to just docID
        _currentStore.write(newRev.record(), t);
    }


    bool RevisionStore::deleteNonCurrent(slice docID, slice revID, Transaction &t) {
        return _nonCurrentStore.del(keyForNonCurrentRevision(docID, Version(revID)), t);
    }


#pragma mark - ENUMERATION:


    RecordEnumerator RevisionStore::enumerateRevisions(slice docID, slice author) {
        static RecordEnumerator::Options kRevEnumOptions;
//FIX:        kRevEnumOptions.inclusiveStart = kRevEnumOptions.inclusiveEnd = false;
        kRevEnumOptions.contentOptions = kMetaOnly;
        
        return RecordEnumerator(_nonCurrentStore,
        /* FIX: Removed RecordEnumerator options used for this
                             startKeyFor(docID, author),
                             endKeyFor(docID, author),
         */
                             kRevEnumOptions);
    }


    std::vector<std::shared_ptr<Revision> > RevisionStore::allOtherRevisions(slice docID) {
        std::vector<std::shared_ptr<Revision> > revs;
        RecordEnumerator e = enumerateRevisions(docID);
        while (e.next()) {
            revs.push_back(std::shared_ptr<Revision>(new Revision(e.record())));
        }
        return revs;
    }


    void RevisionStore::deleteAncestors(Revision &child, Transaction &t) {
        RecordEnumerator e = enumerateRevisions(child.docID());
        while (e.next()) {
            Revision rev(e.record());
            if (rev.version().compareTo(child.version()) == kOlder
                    && !shouldKeepAncestor(rev)) {
                _nonCurrentStore.del(rev.record(), t);
            }
        }
    }


    bool RevisionStore::hasConflictingRevisions(slice docID) {
        RecordEnumerator e = enumerateRevisions(docID);
        while (e.next()) {
            Revision rev(e.record());
            if (!shouldKeepAncestor(rev))
                return true;
        }
        return false;
    }


#pragma mark DOC ID / KEYS:


    // Concatenates the docID, the author and the generation (with delimiters).
    // author and generation are optional.
    static alloc_slice mkkey(slice docID, peerID author, generation gen) {
        size_t size = docID.size + 1;
        if (author.buf) {
            size += author.size + 1;
            if (gen > 0)
                size += fleece::SizeOfVarInt(gen);
        }
        alloc_slice result(size);
        slice out = result;
        out.writeFrom(docID);
        out.writeByte(kDocIDDelimiter);
        if (author.buf) {
            out.writeFrom(author);
            out.writeByte(kAuthorDelimiter);
            if (gen > 0)
                WriteUVarInt(&out, gen);
        }
        return result;
    }

    alloc_slice RevisionStore::keyForNonCurrentRevision(slice docID, class Version vers) {
        return mkkey(docID, vers.author(), vers.gen());
    }

    alloc_slice RevisionStore::startKeyFor(slice docID, peerID author) {
        return mkkey(docID, author, 0);
    }

    alloc_slice RevisionStore::endKeyFor(slice docID, peerID author) {
        alloc_slice result = mkkey(docID, author, 0);
        const_cast<uint8_t&>(result[result.size-1])++;
        return result;
    }

    slice RevisionStore::docIDFromKey(slice key) {
        auto delim = key.findByte(kDocIDDelimiter);
        if (delim)
            key = key.upTo(delim);
        return key;
    }


#pragma mark TO OVERRIDE:

    // These are no-op stubs. CASRevisionStore implements them.

    void RevisionStore::willReplaceCurrentRevision(Revision &, const Revision &, Transaction &t) {
    }

    bool RevisionStore::shouldKeepAncestor(const Revision &rev) {
        return false;
    }

}
