# tifast

tifast is a JVMTI agent designed for profiling the performance impact listening
to various JVMTI events. It is called tifast since none of the event handlers do
anything meaning that it can be considered speed-of-light.

# Usage
### Build
>    `make libtifast`

The libraries will be built for 32-bit, 64-bit, host and target. Below examples
assume you want to use the 64-bit version.

### Command Line

The agent is loaded using -agentpath like normal. It takes arguments in the
following format:
>     `[log,][EventName1[,EventName2[,...]]]`

* If 'log' is the first argument the event handlers will LOG(INFO) when they are
  called. This behavior is static. The no-log methods have no branches and just
  immediately return.

* The event-names are the same names as are used in the jvmtiEventCallbacks
  struct.

* All required capabilities are automatically gained. No capabilities other than
  those needed to listen for the events are gained.

* Only events which do not require additional function calls to cause delivery
  and are sent more than once are supported.

#### Supported events

The following events may be listened for with this agent

* `SingleStep`

* `MethodEntry`

* `MethodExit`

* `NativeMethodBind`

* `Exception`

* `ExceptionCatch`

* `ThreadStart`

* `ThreadEnd`

* `ClassLoad`

* `ClassPrepare`

* `ClassFileLoadHook`

* `CompiledMethodLoad`

* `CompiledMethodUnload`

* `DynamicCodeGenerated`

* `DataDumpRequest`

* `MonitorContendedEnter`

* `MonitorContendedEntered`

* `MonitorWait`

* `MonitorWaited`

* `ResourceExhausted`

* `VMObjectAlloc`

* `GarbageCollectionStart`

* `GarbageCollectionFinish`

All other events cannot be listened for by this agent. Most of these missing
events either require the use of other functions in order to be called
(`FramePop`, `ObjectFree`, etc) or are only called once (`VMInit`, `VMDeath`,
etc).

#### ART
>    `art -Xplugin:$ANDROID_HOST_OUT/lib64/libopenjdkjvmti.so '-agentpath:libtifast.so=MethodEntry' -cp tmp/java/helloworld.dex -Xint helloworld`

* `-Xplugin` and `-agentpath` need to be used, otherwise the agent will fail during init.
* If using `libartd.so`, make sure to use the debug version of jvmti.

>    `adb shell setenforce 0`
>
>    `adb push $ANDROID_PRODUCT_OUT/system/lib64/libtifast.so /data/local/tmp/`
>
>    `adb shell am start-activity --attach-agent /data/local/tmp/libtifast.so=MonitorWait,ClassPrepare some.debuggable.apps/.the.app.MainActivity`

#### RI
>    `java '-agentpath:libtifast.so=MethodEntry' -cp tmp/helloworld/classes helloworld`
