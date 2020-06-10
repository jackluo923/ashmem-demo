#include <jni.h>
#include <string>

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
constexpr char DOMAIN_SOCKET_ABSTRACT_NAMESPACE[] = "\0ABSTRACT_NAMESPACE_NAME";

extern "C"
JNIEXPORT void JNICALL
Java_ca_utoronto_dsrg_ashmem_demo_domainSocketClient_MainActivity_getSharedMemoryRegionNative(JNIEnv *env,
                                                                                              jobject thiz) {
    // Configure unix domain socket
    struct sockaddr_un socketaddr;
    memset(&socketaddr, 0, sizeof(struct sockaddr_un)); // Clear for safety
    socketaddr.sun_family = AF_UNIX; // Unix Domain instead of AF_INET IP domain
    memcpy(&socketaddr.sun_path, DOMAIN_SOCKET_ABSTRACT_NAMESPACE, sizeof(DOMAIN_SOCKET_ABSTRACT_NAMESPACE) - 1);
    int serverSocket = socket(AF_UNIX, SOCK_STREAM, 0);

    // Connect to the specified unix domain socket
    LOGD("Connect to the specified unix domain socket");
    if (connect(serverSocket, (const struct sockaddr *) &socketaddr, sizeof(socketaddr.sun_family) +
            sizeof(DOMAIN_SOCKET_ABSTRACT_NAMESPACE) - 1) < 0) {
        LOGE("server connect: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Receive ashmemFds via SCM_RIGHTS ancillary message
    // http://man7.org/linux/man-pages/man7/unix.7.html
    static constexpr size_t kMessageBufferSize = 4096 * sizeof(int);
    char *kMessageBuf = new char[kMessageBufferSize];
    struct iovec iov[1];
    iov[0].iov_base = kMessageBuf;
    iov[0].iov_len = kMessageBufferSize;
    const int numAshmemFds = 1;
    char fdBuf[CMSG_SPACE(numAshmemFds)];
    struct msghdr msg = {
            .msg_control = fdBuf,
            .msg_controllen = sizeof(fdBuf),
            .msg_iov = &iov[0],
            .msg_iovlen = 1,
    };

    int ret = -1;
    do {
        ret = recvmsg(serverSocket, &msg, 0);
    } while (ret == -1 && errno == EINTR);
    if (ret == -1) {
        ret = errno;
        LOGE("Error reading ashmem handle to socket: error %#x (%s)", ret, strerror(ret));
        exit(EXIT_FAILURE);
    }

    if (msg.msg_iovlen != 1) {
        LOGE("Error reading ashmem handle to socket: bad data length");
        exit(EXIT_FAILURE);
    }
    if (msg.msg_controllen != sizeof(fdBuf)) {
        LOGE("Error reading ashmem handle to socket: bad fdBuf length");
        exit(EXIT_FAILURE);
    }

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (!cmsg) {
        LOGE("Error reading ashmem handle to socket: no fd header");
        exit(EXIT_FAILURE);
    }

    const int *ashmemFds = reinterpret_cast<const int *>(CMSG_DATA(cmsg));
    if (!ashmemFds) {
        LOGE("Error reading ashmem handle to socket: no fd data");
        exit(EXIT_FAILURE);
    }
    LOGE("Received ashmemFd: %d", ashmemFds[0]);

    int64_t* sharedBuffer =
            (int64_t*)mmap(NULL, NUM_SHARED_MEM_SIZE_BYTES, PROT_WRITE, MAP_SHARED, ashmemFds[0], 0);

    // Write share memory information to java member variables
    LOGI("Populating shared memory metadata");
    jclass MainActivityClass = env->GetObjectClass(thiz);
    // Set mRawSharedMemoryPtr
    jfieldID mRawSharedMemoryPtrFieldId =
            env->GetStaticFieldID(MainActivityClass, "mRawSharedMemoryPtr", "J");
    env->SetStaticLongField(MainActivityClass, mRawSharedMemoryPtrFieldId, (jlong)sharedBuffer);
    // Set mRawSharedMemoryFd
    jfieldID mRawSharedMemoryFdFieldId = env->GetStaticFieldID(MainActivityClass, "mRawSharedMemoryFd", "I");
    env->SetStaticIntField(MainActivityClass, mRawSharedMemoryFdFieldId, (jint)ashmemFds[0]);
    // Set mSharedMemoryFd
    jfieldID mSharedMemoryFdFieldId =
            env->GetStaticFieldID(MainActivityClass, "mSharedMemoryFd", "Ljava/io/FileDescriptor;");
    jclass fileDescriptorClass = env->FindClass("java/io/FileDescriptor");
    jmethodID fileDescriptorInitMethodId = env->GetMethodID(fileDescriptorClass, "<init>", "()V");
    jobject mSharedMemoryFd = env->NewObject(fileDescriptorClass, fileDescriptorInitMethodId);
    char discriptorFieldName[] = "descriptor";   // Note: Android renamed "fd" to "descriptor"
    jfieldID descriptorFieldId = env->GetFieldID(fileDescriptorClass, discriptorFieldName, "I");
    env->SetIntField(mSharedMemoryFd, descriptorFieldId, (jint)ashmemFds[0]);
    env->SetStaticObjectField(MainActivityClass, mSharedMemoryFdFieldId, mSharedMemoryFd);
}

extern "C"
JNIEXPORT jlong JNICALL
Java_ca_utoronto_dsrg_ashmem_demo_domainSocketClient_MainActivity_readValueInSharedMemoryNative(JNIEnv *env,
                                                                                                jobject thiz) {
    jclass MainActivityClass = env->GetObjectClass(thiz);
    jfieldID mRawSharedMemoryPtrFieldId =
            env->GetStaticFieldID(MainActivityClass, "mRawSharedMemoryPtr", "J");
    int64_t* sharedBuffer = (int64_t*) env->GetStaticLongField(MainActivityClass, mRawSharedMemoryPtrFieldId);
    return (jlong)sharedBuffer[0];
}