#include <jni.h>

#include <android/log.h>
//#include <android/sharedmem.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/ashmem.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sstream>

#include <android/hardware_buffer.h>

// Android log function wrappers
static const char *kTAG = "ClientIPC";
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
JNIEXPORT jstring JNICALL
Java_ca_utoronto_dsrg_ashmem_demo_client_MainActivity_ashmemReadTimestampNative(JNIEnv *env, jobject thiz) {
    // Obtain shared memory type: 1 -> ashmem, 2 -> AHardwarebuffer
    jclass MainActivityClass = env->GetObjectClass(thiz);
    jfieldID field_sharedMemoryType = env->GetFieldID(MainActivityClass, "sharedMemoryType", "I");
    jint sharedMemoryType = env->GetIntField(thiz, field_sharedMemoryType);

    // Configure unix domain socket
    // Note: Calculate the sockaddr_un stuct sizes carefully: https://stackoverflow.com/a/26361456/3747216
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

    void *shared_buffer = nullptr;
    if (sharedMemoryType == 1) {
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

        const int ashmemSz = sizeof(int64_t);
        shared_buffer = mmap(NULL, ashmemSz, PROT_READ | PROT_WRITE, MAP_SHARED, ashmemFds[0], 0);
    } else {
        AHardwareBuffer *h_buffer = nullptr;
        int ret = AHardwareBuffer_recvHandleFromUnixSocket(serverSocket, &h_buffer);
        if (ret != 0) {
            LOGE("Failed to AHardwareBuffer_recvHandleFromUnixSocket");
        }
        ret = AHardwareBuffer_lock(h_buffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN, -1, NULL, &shared_buffer);
    }

    int shared_memory_value = ((int64_t *) shared_buffer)[0];
    std::ostringstream os;
    os << "Read " << shared_memory_value << " from shared memory";
    const char *log = os.str().c_str();
    LOGE("%s", log);

    std::string value_str = std::to_string(shared_memory_value);
    return env->NewStringUTF(value_str.c_str());
}

extern "C"
JNIEXPORT jobject JNICALL
Java_ca_utoronto_dsrg_ashmem_demo_client_MainActivity_getAshmemJavaFd(JNIEnv *env, jobject thiz) {
    // Obtain shared memory type: 1 -> ashmem, 2 -> AHardwarebuffer
    jclass MainActivityClass = env->GetObjectClass(thiz);
    jfieldID field_sharedMemoryType = env->GetFieldID(MainActivityClass, "sharedMemoryType", "I");
    jint sharedMemoryType = env->GetIntField(thiz, field_sharedMemoryType);

    // Configure unix domain socket
    struct sockaddr_un socketaddr;
    memset(&socketaddr, 0, sizeof(struct sockaddr_un)); // Clear for safety
    socketaddr.sun_family = AF_UNIX; // Unix Domain instead of AF_INET IP domain
    memcpy(&socketaddr.sun_path, DOMAIN_SOCKET_ABSTRACT_NAMESPACE, sizeof(DOMAIN_SOCKET_ABSTRACT_NAMESPACE) - 1);
    int serverSocket = socket(AF_UNIX, SOCK_STREAM, 0);

    // Connect to the specified unix domain socket
    LOGD("Connect to the specified unix domain socket");
    if (connect(serverSocket, (const struct sockaddr *) &socketaddr, sizeof(socketaddr.sun_family) +
                                                                     sizeof(DOMAIN_SOCKET_ABSTRACT_NAMESPACE) - 1) <
        0) {
        LOGE("server connect: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    void *shared_buffer = nullptr;
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

    shared_buffer = mmap(NULL, NUM_SHARED_MEM_SIZE_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, ashmemFds[0], 0);
    int shared_memory_value = ((int64_t *) shared_buffer)[0];
    std::ostringstream os;
    os << "Read " << shared_memory_value << " from shared memory";
    const char *log = os.str().c_str();
    LOGE("%s", log);

    jclass fileDescriptorClass = env->FindClass("java/io/FileDescriptor");
    if (fileDescriptorClass == NULL) {
        LOGE("Unable to find FileDescriptor class");
        return NULL;
    }

    // Construct a new FileDescriptor
    jmethodID fileDescriptorInitMethodId = env->GetMethodID(fileDescriptorClass, "<init>", "()V");
    if (fileDescriptorInitMethodId == NULL) {
        LOGE("Unable to find the <init> method within FileDescriptor class");
        return NULL;
    }
    jobject returnObj = env->NewObject(fileDescriptorClass, fileDescriptorInitMethodId);

    // Poke the "fd" field with the file descriptor. Android renamed "fd" to "descriptor"
    jfieldID descriptorFieldId = env->GetFieldID(fileDescriptorClass, "descriptor", "I");
    if (descriptorFieldId == NULL) {
        LOGE("Unable to find the descriptor member variable within FileDescriptor class <init> method");
        return NULL;
    } else {
        env->SetIntField(returnObj, descriptorFieldId, (jint) ashmemFds[0]);
    }

    return returnObj;
}