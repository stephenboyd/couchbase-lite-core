/**
 * Copyright (c) 2017 Couchbase, Inc. All rights reserved.
 * <p>
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 * <p>
 * http://www.apache.org/licenses/LICENSE-2.0
 * <p>
 * Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific language governing permissions
 * and limitations under the License.
 */
package com.couchbase.litecore;

public class C4QueryOptions {
    //-------------------------------------------------------------------------
    // Member Variables
    //-------------------------------------------------------------------------
    boolean rankFullText = true;

    //-------------------------------------------------------------------------
    // Constructor
    //-------------------------------------------------------------------------
    public C4QueryOptions() {
    }

    public C4QueryOptions(boolean rankFullText) {
        this.rankFullText = rankFullText;
    }

    //-------------------------------------------------------------------------
    // public methods
    //-------------------------------------------------------------------------
    public boolean isRankFullText() {
        return rankFullText;
    }

    public void setRankFullText(boolean rankFullText) {
        this.rankFullText = rankFullText;
    }
}
