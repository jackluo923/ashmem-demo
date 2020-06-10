#include <jni.h>

// Shared memory imports
#include <fcntl.h>
#include <linux/ashmem.h>
#include <sys/ioctl.h>
#include <android/sharedmem.h>
#include <android/hardware_buffer.h>

// Memory map imports
#include <sys/mman.h>

// Unix domain socket imports
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <android/log.h>
#include <cerrno>
#include <cstring>
#include <sstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// Android log function wrappers
static const char *kTAG = "ServerIPC";
#define LOGD(...) \
  ((void)__android_log_print(ANDROID_LOG_DEBUG, kTAG, __VA_ARGS__))
#define LOGI(...) \
  ((void)__android_log_print(ANDROID_LOG_INFO, kTAG, __VA_ARGS__))
#define LOGW(...) \
  ((void)__android_log_print(ANDROID_LOG_WARN, kTAG, __VA_ARGS__))
#define LOGE(...) \
  ((void)__android_log_print(ANDROID_LOG_ERROR, kTAG, __VA_ARGS__))


const int NUM_SHARED_MEM_SIZE_BYTES = sizeof(int64_t);
const int NUM_SHARED_MEM_REGIONS = 1;
constexpr char ASHMEM_REGION_NAME[] = "NOT_DEADBEEF";
constexpr char DOMAIN_SOCKET_ABSTRACT_NAMESPACE[] = "\0ABSTRACT_NAMESPACE_NAME";

extern "C"
JNIEXPORT void JNICALL
Java_ca_utoronto_dsrg_ashmem_demo_server_MainActivity_createSharedMemoryRegionNative(JNIEnv *env, jobject thiz) {
    LOGI("Creating ashmem region using ASharedMemory_create()");
    int sharedMemoryFd = ASharedMemory_create(ASHMEM_REGION_NAME, NUM_SHARED_MEM_SIZE_BYTES);
    int64_t* sharedBuffer =
            (int64_t*)mmap(NULL, NUM_SHARED_MEM_SIZE_BYTES, PROT_WRITE, MAP_SHARED, sharedMemoryFd, 0);

    // Write share memory information to java member variables
    LOGI("Populating shared memory metadata");
    jclass MainActivityClass = env->GetObjectClass(thiz);
    // Set mRawSharedMemoryPtr
    jfieldID mRawSharedMemoryPtrFieldId =
            env->GetStaticFieldID(MainActivityClass, "mRawSharedMemoryPtr", "J");
    env->SetStaticLongField(MainActivityClass, mRawSharedMemoryPtrFieldId, (jlong)sharedBuffer);
    // Set mRawSharedMemoryFd
    jfieldID mRawSharedMemoryFdFieldId = env->GetStaticFieldID(MainActivityClass, "mRawSharedMemoryFd", "I");
    env->SetStaticIntField(MainActivityClass, mRawSharedMemoryFdFieldId, (jint)sharedMemoryFd);
    // Set mSharedMemoryFd
    jfieldID mSharedMemoryFdFieldId =
            env->GetStaticFieldID(MainActivityClass, "mSharedMemoryFd", "Ljava/io/FileDescriptor;");
    jclass fileDescriptorClass = env->FindClass("java/io/FileDescriptor");
    jmethodID fileDescriptorInitMethodId = env->GetMethodID(fileDescriptorClass, "<init>", "()V");
    jobject mSharedMemoryFd = env->NewObject(fileDescriptorClass, fileDescriptorInitMethodId);
    char disriptorFieldName[] = "descriptor";   // Note: Android renamed "fd" to "descriptor"
    jfieldID descriptorFieldId = env->GetFieldID(fileDescriptorClass, disriptorFieldName, "I");
    env->SetIntField(mSharedMemoryFd, descriptorFieldId, (jint)sharedMemoryFd);
    env->SetStaticObjectField(MainActivityClass, mSharedMemoryFdFieldId, mSharedMemoryFd);
}


extern "C"
JNIEXPORT void JNICALL
Java_ca_utoronto_dsrg_ashmem_demo_server_MainActivity_domainSocketServerNative(JNIEnv *env, jobject thiz) {
    // Prepare raw ashmemFds array for sharing across unix domain socket
    jclass MainActivityClass = env->GetObjectClass(thiz);
    jfieldID mRawSharedMemoryFdFieldId = env->GetStaticFieldID(MainActivityClass, "mRawSharedMemoryFd", "I");
    int ashmemFds[NUM_SHARED_MEM_REGIONS] = {env->GetStaticIntField(MainActivityClass, mRawSharedMemoryFdFieldId)};

    // Create, bind and listen to a UNIX domain socket with abstract namespace
    // Note: Calculate the sockaddr_un stuct sizes carefully: https://stackoverflow.com/a/26361456/3747216
    struct sockaddr_un socketaddr = {};
    memset(&socketaddr, 0, sizeof(struct sockaddr_un)); // Clear for safety
    socketaddr.sun_family = AF_UNIX;
    memcpy(&socketaddr.sun_path, DOMAIN_SOCKET_ABSTRACT_NAMESPACE, sizeof(DOMAIN_SOCKET_ABSTRACT_NAMESPACE) - 1);

    int serverSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        LOGE("socket: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    LOGD("Bind to the specified unix domain socket");
    if (bind(serverSocket, (const struct sockaddr*)&socketaddr, sizeof(socketaddr.sun_family) +
            sizeof(DOMAIN_SOCKET_ABSTRACT_NAMESPACE) - 1) < 0) {
        LOGE("socket bind: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    LOGD("Listen on the newly binded unix domain socket");
    if (listen(serverSocket, 8) < 0) {
        LOGE("socket listen: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Prepare the SCM_RIGHTS ancillary message we will send to all clients
    // http://man7.org/linux/man-pages/man7/unix.7.html
    char iovBuf[1];
    struct iovec iov = {
            .iov_base = iovBuf,
            .iov_len = sizeof(iovBuf)
    };
    union {   /* Ancillary data buffer, wrapped in a union
                 in order to ensure it is suitably aligned */
        char buf[CMSG_SPACE(sizeof(ashmemFds))];
        struct cmsghdr align;
    } control;
    struct msghdr msg = {
            .msg_iov = &iov,
            .msg_iovlen = 1,
            .msg_control = control.buf,
            .msg_controllen = sizeof(control.buf)
    };

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int) * NUM_SHARED_MEM_REGIONS);
    memcpy(CMSG_DATA(cmsg), ashmemFds, NUM_SHARED_MEM_REGIONS * sizeof(int));

    int clientSocket;
    while(true) {
        clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket < 0) {
            LOGE("accept: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        LOGI("Accepted a new client on client socketFd %d", clientSocket);
        int ret = -1;
        // For all incoming connections, send the ashmemFd with a SCM_RIGHTS ancillary message
        // http://man7.org/linux/man-pages/man7/unix.7.html
        do {
            ret = sendmsg(clientSocket, &msg, 0);
        } while (ret == -1 && errno == EINTR);
        if (ret == -1) {
            ret = errno;
            LOGE("Error writing ashmem handle to socket: error %#x (%s)",
                 ret, strerror(ret));
            exit(EXIT_FAILURE);
        }
        LOGI("Sent client the ashmemFDs via ancillary message");
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_ca_utoronto_dsrg_ashmem_demo_server_MainActivity_writeValueInSharedMemoryNative(JNIEnv *env, jobject thiz,
                                                                                     jlong shared_memory_value) {
    jclass MainActivityClass = env->GetObjectClass(thiz);
    jfieldID mRawSharedMemoryPtrFieldId =
            env->GetStaticFieldID(MainActivityClass, "mRawSharedMemoryPtr", "J");
    int64_t* sharedBuffer = (int64_t*)env->GetStaticLongField(MainActivityClass, mRawSharedMemoryPtrFieldId);
    sharedBuffer[0] = (int64_t)shared_memory_value;
}

extern "C"
JNIEXPORT jlong JNICALL
Java_ca_utoronto_dsrg_ashmem_demo_server_MainActivity_readValueInSharedMemoryNative(JNIEnv *env, jobject thiz) {
    jclass MainActivityClass = env->GetObjectClass(thiz);
    jfieldID mRawSharedMemoryPtrFieldId =
            env->GetStaticFieldID(MainActivityClass, "mRawSharedMemoryPtr", "J");
    int64_t* sharedBuffer = (int64_t*) env->GetStaticLongField(MainActivityClass, mRawSharedMemoryPtrFieldId);
    return (jlong)sharedBuffer[0];
}
