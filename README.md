# Shared Memory Demo
This is a demo to mapped shared memory region from a server application to client application.
To access server's shared memory region, client obtains the file descriptor either through Unix Domain Sockets or Binder service.
Once the client application receives the file descriptor and maps the shared memory region to its own process, 
it is able to access the shared memory region via JNI or directly in JAVA using NIO direct mapped byte buffer.

## Demo Android Applications
### Server
1. Creates shared memory region using ashmem
2. Creates Unix Domain Socket server to share ashmem file descriptor

   - Note: targetSdkVersion 26 in build.gradle must be set to <= 28, device SDK API version can be higher.
  
3. Creates Binder service to share ashmem file descriptor
4. Set and Get shared memory value


### Domain Socket Client
1. Connects to Unix Domain Socket server

   - Note: targetSdkVersion in build.gradle must be set to <= 27, device SDK API version can be higher.
   
2. Obtains shared memory file descriptor and map shared memory region
3. Reads shared memory value in native via JNI
4. Reads shared memory value in Java's Direct Mapped Byte Buffer (stays in Java and avoids crossing JNI boundry)

   - Setup process is simple but non-trivial. Currently only works for device SDK API version 19-30.


### Binder Client
1. Connects to Binder service implemented in Server

   - Note: targetSdkVersion in build.gradle must be <= 28 to work, device SDK API version can be higher.

2. Obtains shared memory file descriptor and map shared memory region
3. Reads shared memory value in native via JNI
4. Reads shared memory value in Java's Direct Mapped Byte Buffer (stays in Java and avoids crossing JNI boundry)

   - Setup process is simple but non-trivial. Currently only works for device SDK API version 19-30.

## Implementation Discussions
### Issues with Unix Domain Socket and Selinux Enforcement
Google seems to be enforcing selinux unix_stream_socket permission on device API 28 or later.
Therefore, in order for unix stream socket to work, the targetSdkVersion setting in build.gradle file must be set to 27 or lower.
Otherwise, you will observe a similar permission denied error when the client tries to connect to the server below:
```
I/mem.demo.domain-socket-client: type=1400 audit(0.0:257): avc: denied { connectto } for path=0041425354524143545F4E414D4553504143455F4E414D45 scontext=u:r:untrusted_app:s0:c107,c256,c512,c768 tcontext=u:r:untrusted_app:s0:c106,c256,c512,c768 tclass=unix_stream_socket permissive=1 app=ca.utoronto.dsrg.ashmem.demo.domain-socket-client
```
The restriction can also be bypassed by modifying the selinux configuration files in Android's source code with audit2allow or turn off selinux enforcement using adb command ```adb shell setenforce 0```

### Issues With ```FileChannel.map(...)``` and Shared Memory File Descriptor When Mapping Shared Memory To Java Direct Mapped Buffer
This functionaility is completely broken on device SDK API 27 and later for some obscure and unknown reason.

#### Problem Behavior
For ```READ_ONLY``` mapping, Android will throw ```Channel not open for writing - cannot extend file to required size``` error.
From emperical testing, the functionaility worked on emulator image with API26, fails on devices running with API29.

#### Root Cause Analysis
The root cause is that the shared memory file descriptor's "size" is 0 according to fstat64. 
However, this is not the reason for the inability to map a direct buffer to a shared memory file descriptor. 
The true reason is that starting around API29, Android now checks this file size after a code refactorization.

Android-10.0.0_r30 (API29) - FileChannelImpl.java - line: 950-954
- https://cs.android.com/android/platform/superproject/+/android-10.0.0_r30:libcore/ojluni/src/main/java/sun/nio/ch/FileChannelImpl.java

Android-8.0.0_r26 (API26) - FileChannelImpl.java - line: 931-939
- https://cs.android.com/android/platform/superproject/+/android-8.0.0_r26:libcore/ojluni/src/main/java/sun/nio/ch/FileChannelImpl.java

#### Workaround
The usual way of mapping a shared memory region to a shared memory file descriptor is the following:
```java
FileInputStream sharedMemoryFIS = new FileInputStream(<shared memory fd object>)
FileChannel sharedMemoryFC = sharedMemoryFIS.getChannel();
MappedByteBuffer directMappedByteBuffer = sharedMemoryFC.map(...);
```

To map shared memory region to a direct mapped byte buffer, we will need to bypass Google's erroneous logic and utilize Android's hidden API:
```java
final Class<?> directByteBufferClass = Class.forName("java.nio.DirectByteBuffer");
final Constructor<?> directByteBufferConstructor = directByteBufferClass.getConstructor(int.class,
                        long.class, FileDescriptor.class, Runnable.class, boolean.class);
MappedByteBuffer directMappedByteBuffer = (MappedByteBuffer)directByteBufferConstructor.newInstance(
   <shared memory size>, <memory mapped shared memory return address>, <shared memory FileDescriptor object>, null, <Map mode>);
```
Normally accessing hidden API using reflection is restricted. This restriction can be bypassed using a brilliant work called ["ChickenHook"](https://androidreverse.wordpress.com/2020/05/02/android-api-restriction-bypass-for-all-android-versions/
)

#### Working With File Descriptor Shared Via Unix Domain Socket
Unlike FileDescriptor object shared across Binder service via ParcelFileDescriptor, the file descriptor shared across Unix Domain Socket is a simple integer file descriptor in native. To convert integer file descriptor to a Java FileDescriptor object, we can play some tricks inside JNI which bypass Java's access restrictions. We first need to create a java FileDescriptor object, then we manually set the private integer file descriptor via JNI. The code to do this is below:
```java
jfieldID mSharedMemoryFdFieldId =
            env->GetStaticFieldID(MainActivityClass, "mSharedMemoryFd", "Ljava/io/FileDescriptor;");
jclass fileDescriptorClass = env->FindClass("java/io/FileDescriptor");
jmethodID fileDescriptorInitMethodId = env->GetMethodID(fileDescriptorClass, "<init>", "()V");
jobject mSharedMemoryFd = env->NewObject(fileDescriptorClass, fileDescriptorInitMethodId);
char discriptorFieldName[] = "descriptor";   // Note: Android renamed "fd" to "descriptor"
jfieldID descriptorFieldId = env->GetFieldID(fileDescriptorClass, discriptorFieldName, "I");
env->SetIntField(mSharedMemoryFd, descriptorFieldId, (jint)<shared memory int fd>);
```
Note that in Android, the JVM team renamed the private variable "fd" to "descriptor". Their argument is "to avoid issues with JNI/reflection fetching the descriptor value", or trying to prevent exactly what we need to do. Alternatively, we can use Android specific private constructor ```private /* */ FileDescriptor(int descriptor)``` via JNI.

## How To Run
- Each folder is an independent Android Studio project
- Compile and install the APK to emulator or device to run
- Both domain-socket-client and binder-client can be executed simulatenously

## Useful Materials
1. Tutorial on explaining file descriptor sharing across unix domain socket

   - https://medium.com/@spencerfricke/android-ahardwarebuffer-shared-memory-over-unix-domain-sockets-7b27b1271b36
   
2. Tutorial on creating shared memory using ashmem and sharing file descriptor via ParcelFileDescriptor with Binder

   - https://devarea.com/android-creating-shared-memory-using-ashmem/
   
3. Instantiating Java file descriptor with a numbered file descriptor using native code

   - http://www.kfu.com/~nsayer/Java/jni-filedesc.html
   
4. Bypassing API restrictions for Android version 19-30.

   - https://androidreverse.wordpress.com/2020/05/02/android-api-restriction-bypass-for-all-android-versions/
