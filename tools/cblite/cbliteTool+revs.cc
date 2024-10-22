//
//  cbliteTool+revs.cc
//  LiteCore
//
//  Created by Jens Alfke on 9/8/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#include "cbliteTool.hh"


void CBLiteTool::revsUsage() {
    writeUsageCommand("revs", false, "DOCID");
    cerr <<
    "  Shows a document's revision history\n"
    ;
}


void CBLiteTool::revsInfo() {
    // Read params:
    processFlags(nullptr);
    if (_showHelp) {
        revsUsage();
        return;
    }
    openDatabaseFromNextArg();
    string docID = nextArg("document ID");
    endOfArgs();

    auto doc = readDoc(docID);
    if (!doc)
        return;

    cout << "Document \"" << ansiBold() << doc->docID << ansiReset()
         << "\", current revID " << ansiBold() << doc->revID << ansiReset()
         << ", sequence #" << doc->sequence;
    if (doc->flags & kDocDeleted)
        cout << ", Deleted";
    if (doc->flags & kDocConflicted)
        cout << ", Conflicted";
    if (doc->flags & kDocHasAttachments)
        cout << ", Has Attachments";
    cout << "\n";

    // Collect revision tree info:
    RevTree tree;
    alloc_slice root; // use empty slice as root of tree

    do {
        alloc_slice leafRevID = doc->selectedRev.revID;
        alloc_slice childID = leafRevID;
        while (c4doc_selectParentRevision(doc)) {
            alloc_slice parentID = doc->selectedRev.revID;
            tree[parentID].insert(childID);
            childID = parentID;
        }
        tree[root].insert(childID);
        c4doc_selectRevision(doc, leafRevID, false, nullptr);
    } while (c4doc_selectNextLeafRevision(doc, true, true, nullptr));

    writeRevisionChildren(doc, tree, root, "");
}


void CBLiteTool::writeRevisionTree(C4Document *doc,
                       RevTree &tree,
                       alloc_slice root,
                       const string &indent)
{
    static const char* const kRevFlagName[7] = {
        "Deleted", "Leaf", "New", "Attach", "KeepBody", "Conflict", "Foreign"
    };
    C4Error error;
    if (!c4doc_selectRevision(doc, root, true, &error))
        fail("accessing revision", error);
    auto &rev = doc->selectedRev;
    cout << indent << "* ";
    if (rev.flags & kRevLeaf)
        cout << ansiBold();
    cout << rev.revID << ansiReset() << " (#" << rev.sequence << ")";
    if (rev.body.buf)
        cout << ", " << rev.body.size << " bytes";
    for (int bit = 0; bit < 7; bit++) {
        if (rev.flags & (1 << bit))
            cout << ", " << kRevFlagName[bit];
    }
    cout << "\n";
    writeRevisionChildren(doc, tree, root, indent + "  ");
}

void CBLiteTool::writeRevisionChildren(C4Document *doc,
                           RevTree &tree,
                           alloc_slice root,
                           const string &indent)
{
    auto &children = tree[root];
    for (auto i = children.rbegin(); i != children.rend(); ++i) {
        writeRevisionTree(doc, tree, *i, indent);
    }
}


