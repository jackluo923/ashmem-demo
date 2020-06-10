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

   - Note: targetSdkVersion 26 in build.gradle must be set to <= 28, device SDK API version can be higher.
   
2. Obtains shared memory file descriptor and map shared memory region
3. Reads shared memory value in native via JNI
4. Reads shared memory value in Java's Direct Mapped Byte Buffer (stays in Java and avoids crossing JNI boundry)

   - Setup process is simple but non-trivial. Currently only works for device SDK API version 19-30.

### Binder Client
1. Connects to Binder service implemented in Server

   - Note: targetSdkVersion 26 in build.gradle must be <= 28 to work, device SDK API version can be higher.

2. Obtains shared memory file descriptor and map shared memory region
3. Reads shared memory value in native via JNI
4. Reads shared memory value in Java's Direct Mapped Byte Buffer (stays in Java and avoids crossing JNI boundry)

   - Setup process is simple but non-trivial. Currently only works for device SDK API version 19-30.
   
   
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
