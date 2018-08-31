# fieldnull

fieldnull is a JVMTI agent designed for testing for a given field the number of
instances with that field set to null. This can be useful for determining what
fields should be moved into side structures in cases where memory use is
important.

# Usage
### Build
>    `make libfieldnull`

The libraries will be built for 32-bit, 64-bit, host and target. Below examples
assume you want to use the 64-bit version.

### Command Line

The agent is loaded using -agentpath like normal. It takes arguments in the
following format:
>     `Lname/of/class;.nameOfField:Ltype/of/field;[,...]`

#### ART
>    `art -Xplugin:$ANDROID_HOST_OUT/lib64/libopenjdkjvmti.so '-agentpath:libfieldnull.so=Lname/of/class;.nameOfField:Ltype/of/field;' -cp tmp/java/helloworld.dex -Xint helloworld`

* `-Xplugin` and `-agentpath` need to be used, otherwise the agent will fail during init.
* If using `libartd.so`, make sure to use the debug version of jvmti.

>    `adb shell setenforce 0`
>
>    `adb push $ANDROID_PRODUCT_OUT/system/lib64/libfieldnull.so /data/local/tmp/`
>
>    `adb shell am start-activity --attach-agent '/data/local/tmp/libfieldnull.so=Ljava/lang/Class;.name:Ljava/lang/String;' some.debuggable.apps/.the.app.MainActivity`

#### RI
>    `java '-agentpath:libfieldnull.so=Lname/of/class;.nameOfField:Ltype/of/field;' -cp tmp/helloworld/classes helloworld`

### Printing the Results
All statistics gathered during the trace are printed automatically when the
program normally exits. In the case of Android applications, they are always
killed, so we need to manually print the results.

>    `kill -SIGQUIT $(pid com.littleinc.orm_benchmark)`

Will initiate a dump of the counts (to logcat).

The dump will look something like this.

> `dalvikvm32 I 08-30 14:51:20 84818 84818 fieldnull.cc:96] Dumping counts of null fields.`
>
> `dalvikvm32 I 08-30 14:51:20 84818 84818 fieldnull.cc:97] 	Field name	null count	total count`
>
> `dalvikvm32 I 08-30 14:51:20 84818 84818 fieldnull.cc:135] 	Ljava/lang/Class;.name:Ljava/lang/String;	5	2936`
