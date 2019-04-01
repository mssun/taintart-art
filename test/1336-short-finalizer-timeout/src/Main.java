/*
 * Copyright (C) 2007 The Android Open Source Project
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

import dalvik.system.VMRuntime;
import java.util.concurrent.CountDownLatch;
import static java.util.concurrent.TimeUnit.MINUTES;

/**
 * Test a class with a bad finalizer in an environment with a short finalizer timeout.
 *
 * This test is inherently flaky. It assumes that the system will schedule the finalizer daemon
 * and finalizer watchdog daemon enough to reach the timeout and throwing the fatal exception.
 * Largely cloned from 030-bad-finalizer.
 */
public class Main {
    public static void main(String[] args) throws Exception {
        CountDownLatch finalizerWait = new CountDownLatch(1);

        System.out.println("Finalizer timeout = "
                + VMRuntime.getRuntime().getFinalizerTimeoutMs() + " msecs.");
        // A separate method to ensure no dex register keeps the object alive.
        createBadFinalizer(finalizerWait);

        for (int i = 0; i < 2; i++) {
            Runtime.getRuntime().gc();
        }

        // Now wait for the finalizer to start running.
        if (!finalizerWait.await(1, MINUTES)) {
            System.out.println("finalizerWait timed out.");
        }

        // Sleep for less time than the default finalizer timeout, but significantly more than
        // the new timeout plus the 5 seconds we wait to dump thread stacks before actually
        // exiting.
        snooze(9800);

        // We should not get here, since it should only take 5.5 seconds for the timed out
        // fiinalizer to kill the process.
        System.out.println("UNREACHABLE");
        System.exit(0);
    }

    private static void createBadFinalizer(CountDownLatch finalizerWait) {
        BadFinalizer bf = new BadFinalizer(finalizerWait);

        System.out.println("About to null reference.");
        bf = null;  // Not that this would make a difference, could be eliminated earlier.
    }

    public static void snooze(int ms) {
        try {
            Thread.sleep(ms);
        } catch (InterruptedException ie) {
            System.out.println("Snooze(" + ms + ") interrupted");
        }
    }

    /**
     * Class with a bad finalizer.
     */
    public static class BadFinalizer {
        private CountDownLatch finalizerWait;
        private volatile int j = 0;  // Volatile in an effort to curb loop optimization.

        public BadFinalizer(CountDownLatch finalizerWait) {
            this.finalizerWait = finalizerWait;
        }

        protected void finalize() {
            System.out.println("Finalizer started and snoozing...");
            finalizerWait.countDown();
            snooze(200);
            System.out.println("Finalizer done snoozing.");

            System.out.println("Finalizer sleeping forever now.");
            while (true) {
                snooze(10000);
            }
        }
    }
}
