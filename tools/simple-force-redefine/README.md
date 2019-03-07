# forceredfine

ForceRedefine is a JVMTI agent designed for testing how redefiniton affects running processes. It
allows one to force classes to be redefined by writing to a fifo or give a process a list of
classes to try redefining. Currently the redefinition is limited to adding (or removing) a single
NOP at the beginning of every function in the class.

# Usage
### Build
>    `make libforceredefine`

The libraries will be built for 32-bit, 64-bit, host and target. Below examples
assume you want to use the 64-bit version.

#### ART
>    `adb shell setenforce 0`
>
>    `adb push $ANDROID_PRODUCT_OUT/system/lib64/libforceredefine.so /data/local/tmp/`
>
>    `echo java/util/ArrayList > /tmp/classlist`
>    `echo java/util/Arrays >> /tmp/classlist`
>    `adb push /tmp/classlist /data/local/tmp/`
>
>    `adb shell am attach-agent $(adb shell pidof some.deubggable.app) /data/local/tmp/libforceredefine.so=/data/local/tmp/classlist`

Since the agent has no static state it can be attached multiple times to the same process.

>    `adb shell am attach-agent $(adb shell pidof some.deubggable.app) /data/local/tmp/libforceredefine.so=/data/local/tmp/classlist`
>    `adb shell am attach-agent $(adb shell pidof some.deubggable.app) /data/local/tmp/libforceredefine.so=/data/local/tmp/classlist2`
>    `adb shell am attach-agent $(adb shell pidof some.deubggable.app) /data/local/tmp/libforceredefine.so=/data/local/tmp/classlist`

One can also use fifos to send classes interactively to the process. (TODO: Have the agent
continue reading from the fifo even after it gets an EOF.)