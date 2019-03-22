# dumpjvmti

dumpjvmti is a JVMTI agent designed for helping debug the working of the openjdkjvmti plugin. It
allows one to use SIGQUIT to dump information about the current JVMTI state to logcat. It does
this by calling the com.android.art.misc.get_plugin_internal_state extension function.

# Usage
### Build
>    `make libdumpjvmti`

The libraries will be built for 32-bit, 64-bit, host and target. Below examples
assume you want to use the 64-bit version.

#### ART
>    `art -Xplugin:$ANDROID_HOST_OUT/lib64/libopenjdkjvmti.so '-agentpath:libdumpjvmti.so' -cp tmp/java/helloworld.dex -Xint helloworld`
>    `kill -3 <pid>`

* `-Xplugin` and `-agentpath` need to be used, otherwise the agent will fail during init.
* If using `libartd.so`, make sure to use the debug version of jvmti.

>    `adb shell setenforce 0`
>
>    `adb push $ANDROID_PRODUCT_OUT/system/lib64/libdumpjvmti.so /data/local/tmp/`
>
>    `adb shell am start-activity --attach-agent /data/local/tmp/libdumpjvmti.so some.debuggable.apps/.the.app.MainActivity`
>
>    `adb shell kill -3 $(adb shell pidof some.debuggable.apps)`