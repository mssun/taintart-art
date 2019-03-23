# jitload

Jitload is an art-specific agent allowing one to count the number of classes
loaded on the jit-thread or verify that none were.

# Usage
### Build
>    `make libjitload`  # or 'make libjitloadd' with debugging checks enabled

The libraries will be built for 32-bit, 64-bit, host and target. Below examples assume you want to use the 64-bit version.
### Command Line

>    `art -Xplugin:$ANDROID_HOST_OUT/lib64/libopenjdkjvmti.so -agentpath:$ANDROID_HOST_OUT/lib64/libjitload.so -cp tmp/java/helloworld.dex -Xint helloworld`

* `-Xplugin` and `-agentpath` need to be used, otherwise libtitrace agent will fail during init.
* If using `libartd.so`, make sure to use the debug version of jvmti and agent.
* Pass the '=fatal' option to the agent to cause it to abort if any classes are
  loaded on a jit thread. Otherwise a warning will be printed.

>    `art -d -Xplugin:$ANDROID_HOST_OUT/lib64/libopenjdkjvmtid.so -agentpath:$ANDROID_HOST_OUT/lib64/libjitloadd.so=fatal -cp tmp/java/helloworld.dex -Xint helloworld`

* To use with run-test or testrunner.py use the --with-agent argument.

>    `./test/run-test --host --with-agent libtitraced.so=fatal 001-HelloWorld`


### Printing the Results
All statistics gathered during the trace are printed automatically when the
program normally exits. In the case of Android applications, they are always
killed, so we need to manually print the results.

>    `kill -SIGQUIT $(pid com.example.android.displayingbitmaps)`

Will initiate a dump of the counts (to logcat).

