//
//  RevTree.hh
//  Couchbase Lite Core
//
//  Created by Jens Alfke on 5/13/14.
//  Copyright (c) 2014-2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#pragma once
#include "PlatformCompat.hh"
#include "slice.hh"
#include "RevID.hh"
#include <deque>
#include <vector>


namespace litecore {

    class RevTree;

    /** In-memory representation of a single revision's metadata. */
    class Rev {
    public:
        const RevTree*  owner;
        const Rev*      parent;
        revid           revID;      /**< Revision ID (compressed) */
        sequence_t      sequence;   /**< DB sequence number that this revision has/had */

        slice body() const          {return _body;}
        bool isBodyAvailable() const{return _body.buf != nullptr;}

        bool isLeaf() const         {return (flags & kLeaf) != 0;}
        bool isDeleted() const      {return (flags & kDeleted) != 0;}
        bool hasAttachments() const {return (flags & kHasAttachments) != 0;}
        bool isNew() const          {return (flags & kNew) != 0;}
        bool isConflict() const     {return (flags & kIsConflict) != 0;}
        bool isForeign() const      {return (flags & kForeign) != 0;}
        bool isActive() const       {return isLeaf() && !isDeleted();}

        unsigned index() const;
        const Rev* next() const;       // next by order in array, i.e. descending priority
        std::vector<const Rev*> history() const;

        bool operator< (const Rev& rev) const;

        enum Flags : uint8_t {
            kDeleted        = 0x01, /**< Is this revision a deletion/tombstone? */
            kLeaf           = 0x02, /**< Is this revision a leaf (no children?) */
            kNew            = 0x04, /**< Has this rev been inserted since decoding? */
            kHasAttachments = 0x08, /**< Does this rev's body contain attachments? */
            kKeepBody       = 0x10, /**< Body will not be discarded after I'm a non-leaf */
            kIsConflict     = 0x20, /**< Unresolved conflicting revision; should never be current */
            kForeign        = 0x40, /**< Rev originated on a remote peer */
            // Keep these flags consistent with C4RevisionFlags, in c4Document.h!
        };
        Flags flags;

    private:
        slice       _body;          /**< Revision body (JSON), or empty if not stored in this tree*/

        void addFlag(Flags f)           {flags = (Flags)(flags | f);}
        void clearFlag(Flags f)         {flags = (Flags)(flags & ~f);}
        void removeBody()               {clearFlag((Flags)(kKeepBody | kHasAttachments));
                                         _body = nullslice;}
        void markForPurge()             {revID.setSize(0);}
        bool isMarkedForPurge() const   {return revID.size == 0;}
#if DEBUG
        void dump(std::ostream&);
#endif
        friend class RevTree;
        friend class RawRevision;
    };


    /** A serializable tree of Revisions. */
    class RevTree {
    public:
        RevTree() { }
        RevTree(slice raw_tree, sequence_t seq);
        RevTree(const RevTree&);
        virtual ~RevTree() { }

        void decode(slice raw_tree, sequence_t seq);

        alloc_slice encode();

        size_t size() const                             {return _revs.size();}
        const Rev* get(unsigned index) const;
        const Rev* get(revid) const;
        const Rev* operator[](unsigned index) const {return get(index);}
        const Rev* operator[](revid revID) const    {return get(revID);}
        const Rev* getBySequence(sequence_t) const;

        const std::vector<Rev*>& allRevisions() const   {return _revs;}
        const Rev* currentRevision();
        bool hasConflict() const;

        // Adds a new leaf revision, given the parent's revID
        const Rev* insert(revid,
                          slice body,
                          Rev::Flags,
                          revid parentRevID,
                          bool allowConflict,
                          int &httpStatus);

        // Adds a new leaf revision, given a pointer to the parent Rev
        const Rev* insert(revid,
                          slice body,
                          Rev::Flags,
                          const Rev* parent,
                          bool allowConflict,
                          int &httpStatus);

        // Adds a new leaf revision along with any new ancestor revs in its history.
        // (history[0] is the new rev's ID, history[1] is its parent's, etc.)
        int insertHistory(const std::vector<revidBuffer> history,
                          slice body,
                          Rev::Flags);

        unsigned prune(unsigned maxDepth);

        void removeBody(const Rev* NONNULL);

        void removeNonLeafBodies();

        /** Removes a leaf revision and any of its ancestors that aren't shared with other leaves. */
        int purge(revid);
        int purgeAll();

        void markCurrentRevision(Rev::Flags f)    {const_cast<Rev*>(currentRevision())->addFlag(f);}

        void sort();

        void saved(sequence_t newSequence);

#if DEBUG
        void dump();
#endif

    protected:
        virtual bool isBodyOfRevisionAvailable(const Rev* r NONNULL) const;
        virtual alloc_slice readBodyOfRevision(const Rev* r NONNULL) const;
#if DEBUG
        virtual void dump(std::ostream&);
#endif

    private:
        friend class Rev;
        void initRevs();
        Rev* _insert(revid, slice body, Rev *parentRev, Rev::Flags);
        bool confirmLeaf(Rev* testRev NONNULL);
        void compact();
        void checkForResolvedConflict();

        bool                     _sorted {true};         // Are the revs currently sorted?
        std::vector<Rev*>        _revs;
        std::vector<alloc_slice> _insertedData;
    protected:
        std::deque<Rev> _revsStorage;               // Actual storage of the Rev objects
        bool _changed {false};
        bool _unknown {false};
    };

}
