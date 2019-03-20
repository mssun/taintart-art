/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

class StringLiterals {
    static class StartupClass {
        static {
            System.out.println("Startup init");
        }
    }

    static class OtherClass {
        static {
            System.out.println("Other class init");
        }
    }

    void startUpMethod() {
        String resource = "abcd.apk";
        System.out.println("Starting up");
        System.out.println("Loading " + resource);
    }

    static class InnerClass {
        void startUpMethod2() {
            String resource = "ab11.apk";
            System.out.println("Start up method 2");
        }
    }

    void otherMethod() {
        System.out.println("Unexpected error");
        System.out.println("Shutting down!");
    }
}
