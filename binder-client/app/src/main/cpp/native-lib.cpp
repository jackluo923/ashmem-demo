#include <jni.h>
#include <string>

// Memory map imports
#include <sys/mman.h>

const int NUM_SHARED_MEM_SIZE_BYTES = sizeof(int64_t);

extern "C"
JNIEXPORT void JNICALL
Java_ca_utoronto_dsrg_ashmem_demo_server_MainActivity_getSharedMemoryRegionNative(JNIEnv *env, jobject thiz) {
    // Get mRawSharedMemoryFd and memory map a memory region
    jclass MainActivityClass = env->GetObjectClass(thiz);
    jfieldID mRawSharedMemoryFdFieldId = env->GetStaticFieldID(MainActivityClass, "mRawSharedMemoryFd", "I");
    int sharedMemoryFd = (int)env->GetStaticIntField(MainActivityClass, mRawSharedMemoryFdFieldId);
    int64_t* sharedBuffer =
            (int64_t*)mmap(NULL, NUM_SHARED_MEM_SIZE_BYTES, PROT_WRITE, MAP_SHARED, sharedMemoryFd, 0);
    // Set mRawSharedMemoryPtr
    jfieldID mRawSharedMemoryPtrFieldId =
            env->GetStaticFieldID(MainActivityClass, "mRawSharedMemoryPtr", "J");
    env->SetStaticLongField(MainActivityClass, mRawSharedMemoryPtrFieldId, (jlong)sharedBuffer);
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