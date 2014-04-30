//
//  CBForestIndex.m
//  CBForest
//
//  Created by Jens Alfke on 4/1/14.
//  Copyright (c) 2014 Couchbase. All rights reserved.
//

#import "CBForestIndex.h"
#import "CBForestPrivate.h"
#import "CBCollatable.h"
#import "varint.h"
#import <forestdb.h>


id kCBForestIndexNoValue;


@implementation CBForestIndex


+ (void) initialize {
    if (!kCBForestIndexNoValue)
        kCBForestIndexNoValue = [[NSObject alloc] init];
}


- (BOOL) _removeOldRowsForDoc: (NSData*)collatebleDocID {
    NSData* oldSeqData;
    if (![self getValue: &oldSeqData meta: NULL forKey: collatebleDocID error: NULL])
        return NO;
    if (!oldSeqData.length)
        return NO;
    // Decode a series of sequences from packed varint data:
    slice seqBuf = DataToSlice(oldSeqData);
    uint64_t seq;
    while (ReadUVarInt(&seqBuf, &seq)) {
        // ...and delete the old key/values with those sequences:
        [self deleteSequence: seq error: NULL];
    }
    return YES;
}


- (void) _recordNewRows: (NSArray*)sequences forDoc: (NSData*)collatableDocID {
    // Encode the new sequences into a packed series of varints:
    NSMutableData* seqData = nil;
    if (sequences.count) {
        seqData = [NSMutableData dataWithLength: sequences.count*kMaxVarintLen64];
        slice seqBuf = DataToSlice(seqData);
        for (NSNumber* seq in sequences)
            WriteUVarInt(&seqBuf, seq.unsignedLongLongValue);
        seqData.length = seqBuf.buf - seqData.mutableBytes;
    }
    [self setValue: seqData
              meta: nil
            forKey: collatableDocID
             error: NULL];

}


- (BOOL) setKeys: (NSArray*)keys
          values: (NSArray*)values
     forDocument: (NSString*)docID
      atSequence: (CBForestSequence)docSequence
           error: (NSError**)outError
{
    return [self inTransaction: ^BOOL {
        // Remove any old key/value pairs previously generated by this document:
        NSData* collatableDocID = CBCreateCollatable(docID);
        BOOL hadRows = [self _removeOldRowsForDoc: collatableDocID];

        // Add the key/value pairs:
        NSMutableArray* seqs = nil;
        NSUInteger count = keys.count;
        if (count > 0) {
            seqs = [[NSMutableArray alloc] initWithCapacity: count];
            NSMutableData* keyData = [NSMutableData dataWithCapacity: 1024];
            for (NSUInteger i = 0; i < count; i++) {
                @autoreleasepool {
                    keyData.length = 0;
                    CBCollatableBeginArray(keyData);
                    CBAddCollatable(keys[i], keyData);
                    CBAddCollatable(docID, keyData);
                    CBAddCollatable(@(docSequence), keyData);
                    CBCollatableEndArray(keyData);

                    NSData* bodyData;
                    id value = values[i];
                    if (value != kCBForestIndexNoValue) {
                        bodyData = JSONToData(value, NULL);
                        if (!bodyData) {
                            NSLog(@"WARNING: Can't index non-JSON value %@", value);
                            continue;
                        }
                    } else {
                        bodyData = [NSData data];
                    }

                    CBForestSequence seq = [self setValue: bodyData
                                                     meta: nil
                                                   forKey: keyData
                                                    error: outError];
                    if (seq == kCBForestNoSequence)
                        return NO;
                    [seqs addObject: @(seq)];
                    //NSLog(@"INDEX: Seq %llu = %@ --> %@", seq, keyData, body);
                }
            }
        }

        // Update the list of sequences used for this document:
        if (hadRows || seqs.count > 0)
            [self _recordNewRows: seqs forDoc: collatableDocID];
        return YES;
    }];
}


@end




@implementation CBForestQueryEnumerator
{
    CBForestIndex* _index;
    CBForestEnumerationOptions _options;
    NSEnumerator* _indexEnum;
    id _stopBeforeKey;
    NSEnumerator* _keys;
    NSData* _valueData;
    id _value;
}

@synthesize key=_key, valueData=_valueData, docID=_docID, sequence=_sequence, error=_error;


- (instancetype) initWithIndex: (CBForestIndex*)index
                      startKey: (id)startKey
                    startDocID: (NSString*)startDocID
                        endKey: (id)endKey
                      endDocID: (NSString*)endDocID
                       options: (const CBForestEnumerationOptions*)options
                         error: (NSError**)outError
{
    self = [super init];
    if (self) {
        _index = index;
        _options = options ? *options : kCBForestEnumerationOptionsDefault;

        // Remember, the underlying keys are of the form [emittedKey, docID, serial#]
        NSMutableArray* realStartKey = [NSMutableArray arrayWithObjects: startKey, startDocID, nil];
        NSMutableArray* realEndKey = [NSMutableArray arrayWithObjects: endKey, endDocID, nil];
        NSMutableArray* maxKey = (options && options->descending) ? realStartKey : realEndKey;
        [maxKey addObject: @{}];

        _stopBeforeKey = (options && !options->inclusiveEnd) ? endKey : nil;

        if (![self iterateFromKey: realStartKey toKey: realEndKey error: outError])
            return nil;
    }
    return self;
}


- (instancetype) initWithIndex: (CBForestIndex*)index
                          keys: (NSEnumerator*)keys
                       options: (const CBForestEnumerationOptions*)options
                         error: (NSError**)outError
{
    self = [super init];
    if (self) {
        _index = index;
        _options = options ? *options : kCBForestEnumerationOptionsDefault;
        _keys = keys;
        if (![self nextKey]) {
            if (outError)
                *outError = _error;
            return nil;
        }
    }
    return self;
}


- (BOOL) iterateFromKey: (id)realStartKey toKey: (id)realEndKey error: (NSError**)outError {
    _indexEnum = [_index enumerateDocsFromKey: CBCreateCollatable(realStartKey)
                                        toKey: CBCreateCollatable(realEndKey)
                                      options: &_options
                                        error: outError];
    return (_indexEnum != nil);
}


// go on to the next key in the array
- (BOOL) nextKey {
    id key = _keys.nextObject;
    if (!key)
        return NO;
    NSError* error;
    if (![self iterateFromKey: @[key] toKey: @[key, @{}] error: &error]) {
        _error = error;
        return NO;
    }
    return YES;
}


- (id) nextObject {
    _error = nil;

    CBForestDocument* doc;
    do {
        doc = _indexEnum.nextObject;
        if (!doc && ![self nextKey])
            return nil;
    } while (!doc);

    // Decode the key from collatable form:
    slice indexKey = doc.rawID;
    id key;
    NSString* docID;
    int64_t docSequence;
    CBCollatableReadNext(&indexKey, NO, &key); // array marker
    CBCollatableReadNext(&indexKey, YES, &key);
    CBCollatableReadNext(&indexKey, NO, &docID);
    CBCollatableReadNextNumber(&indexKey, &docSequence);
    NSAssert(key && docID, @"Bogus view key");

    if ([_stopBeforeKey isEqual: key])
        return nil;

    // Decode the value:
    NSData* valueData = nil;
    if (doc.bodyLength > 0) {
        NSError* error;
        valueData = [doc readBody: &error];
        if (!valueData) {
            _error = error;
            return nil;
        }
    }

    _key = key;
    _docID = docID;
    _value = nil;
    _valueData = valueData;
    _sequence = docSequence;
    return _key;
}


- (id) value {
    if (!_value && _valueData) {
        _value = [NSJSONSerialization JSONObjectWithData: _valueData
                                                 options: NSJSONReadingAllowFragments
                                                   error: NULL];
    }
    return _value;
}

@end