/** Copyright 2020-2022 Alibaba Group Holding Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package io.v6d.core.common.util;

import static org.junit.Assert.assertEquals;

import org.junit.Test;

/** Unit test for Vineyard ObjectID. */
public class IdTest {
    @Test
    public void roundtrip() {
        ObjectID id = new ObjectID(1000L);
        assertEquals(id, ObjectID.fromString(id.toString()));
    }
}
