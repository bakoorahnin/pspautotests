#include "shared.h"
#include <limits.h>

#define LOCK_TEST_SIMPLE(title, workareaPtr, count) { \
	int result = sceKernelTryLockLwMutex_600(workareaPtr, count); \
	if (result == 0) { \
		printf("%s: OK\n", title); \
	} else { \
		printf("%s: Failed (%X)\n", title, result); \
	} \
}

#define LOCK_TEST(title, attr, initial, count) { \
	SceLwMutexWorkarea workarea; \
	int result = sceKernelCreateLwMutex(&workarea, "lock", attr, initial, NULL); \
	if (result == 0) { \
		LOCK_TEST_SIMPLE(title, &workarea, count); \
	} else { \
		printf("%s: Failed (%X)\n", title, result); \
	} \
	printfLwMutex(&workarea); \
	sceKernelDeleteLwMutex(&workarea); \
	FAKE_LWMUTEX(workarea, attr, initial); \
	LOCK_TEST_SIMPLE(title " (fake)", &workarea, count); \
	printfLwMutex(&workarea); \
}

#define LOCK_TEST_THREAD(title, attr, initial, count) { \
	printf("%s: ", title); \
	schedulingLogPos = 0; \
	schedulingResult = -1; \
	SceLwMutexWorkarea workarea; \
	sceKernelCreateLwMutex(&workarea, "lock", attr, initial, NULL); \
	void *workareaPtr = &workarea; \
	sceKernelStartThread(lockThread, sizeof(void*), &workareaPtr); \
	sceKernelDelayThread(400); \
	int result = sceKernelTryLockLwMutex_600(&workarea, count); \
	schedulingLogPos += sprintf(schedulingLog + schedulingLogPos, "L2 "); \
	sceKernelDelayThread(600); \
	sceKernelDeleteLwMutex(&workarea); \
	sceKernelWaitThreadEnd(lockThread, NULL); \
	schedulingLog[schedulingLogPos] = 0; \
	schedulingLogPos = 0; \
	if (result == 0) { \
		printf("%sOK (thread=%08X)\n", schedulingLog, schedulingResult); \
	} else { \
		printf("%sFailed (thread=%08X, main=%08X)\n", schedulingLog, schedulingResult, result); \
	} \
	sceKernelTerminateThread(lockThread); \
	\
	FAKE_LWMUTEX(workarea, attr, initial); \
	printf("%s (fake): ", title); \
	schedulingLogPos = 0; \
	schedulingResult = -1; \
	sceKernelStartThread(lockThread, sizeof(void*), &workareaPtr); \
	sceKernelDelayThread(400); \
	result = sceKernelTryLockLwMutex_600(&workarea, count); \
	schedulingLogPos += sprintf(schedulingLog + schedulingLogPos, "L2 "); \
	sceKernelDelayThread(600); \
	sceKernelWaitThreadEnd(lockThread, NULL); \
	if (result == 0) { \
		printf("%sOK (thread=%08X)\n", schedulingLog, schedulingResult); \
	} else { \
		printf("%sFailed (thread=%08X, main=%08X)\n", schedulingLog, schedulingResult, result); \
	} \
	sceKernelTerminateThread(lockThread); \
}

static int lockFunc(SceSize argSize, void* argPointer) {
	SceUInt timeout = 1000;
	schedulingResult = sceKernelLockLwMutex(*(void**) argPointer, 1, &timeout);
	schedulingLogPos += sprintf(schedulingLog + schedulingLogPos, "L1 "); \
	sceKernelDelayThread(1000);
	if (schedulingResult == 0)
		sceKernelUnlockLwMutex(*(void**) argPointer, 1);
	return 0;
}

static int deleteMeFunc(SceSize argSize, void* argPointer) {
	int result = sceKernelTryLockLwMutex_600(*(void**) argPointer, 1);
	printf("After delete: %08X\n", result);
	return 0;
}

int main(int argc, char **argv) {
	LOCK_TEST("Lock 0 => 0", PSP_MUTEX_ATTR_FIFO, 0, 0);
	LOCK_TEST("Lock 0 => 1", PSP_MUTEX_ATTR_FIFO, 0, 1);
	LOCK_TEST("Lock 0 => 2", PSP_MUTEX_ATTR_FIFO, 0, 2);
	LOCK_TEST("Lock 0 => -1", PSP_MUTEX_ATTR_FIFO, 0, -1);
	LOCK_TEST("Lock 1 => 1", PSP_MUTEX_ATTR_FIFO, 1, 1);
	LOCK_TEST("Lock 0 => 2 (recursive)", PSP_MUTEX_ATTR_ALLOW_RECURSIVE, 0, 2);
	LOCK_TEST("Lock 0 => -1 (recursive)", PSP_MUTEX_ATTR_ALLOW_RECURSIVE, 0, -1);
	LOCK_TEST("Lock 1 => 1 (recursive)", PSP_MUTEX_ATTR_ALLOW_RECURSIVE, 1, 1);
	LOCK_TEST("Lock 1 => INT_MAX - 1 (recursive)", PSP_MUTEX_ATTR_ALLOW_RECURSIVE, 1, INT_MAX - 1);
	LOCK_TEST("Lock 1 => INT_MAX (recursive)", PSP_MUTEX_ATTR_ALLOW_RECURSIVE, 1, INT_MAX);
	LOCK_TEST("Lock INT_MAX => INT_MAX (recursive)", PSP_MUTEX_ATTR_ALLOW_RECURSIVE, INT_MAX, INT_MAX);

	SceUID lockThread = CREATE_SIMPLE_THREAD(lockFunc);
	LOCK_TEST_THREAD("Locked 1 => 1", PSP_MUTEX_ATTR_FIFO, 1, 1);
	LOCK_TEST_THREAD("Locked 0 => 1", PSP_MUTEX_ATTR_FIFO, 0, 1);
	LOCK_TEST_THREAD("Locked 1 => 1 (recursive)", PSP_MUTEX_ATTR_ALLOW_RECURSIVE, 1, 1);
	LOCK_TEST_THREAD("Locked 0 => 1 (recursive)", PSP_MUTEX_ATTR_ALLOW_RECURSIVE, 0, 1);

	// Probably we can't manage to delete it at the same time.
	SceUID deleteThread = CREATE_SIMPLE_THREAD(deleteMeFunc);
	SceLwMutexWorkarea workarea;
	sceKernelCreateLwMutex(&workarea, "lock", 0, 1, NULL);
	void *workareaPtr = &workarea;
	sceKernelStartThread(deleteThread, sizeof(int), &workareaPtr);
	sceKernelDeleteLwMutex(&workarea);

	// Crashes.
	//LOCK_TEST_SIMPLE("NULL => 0", 0, 0);
	//LOCK_TEST_SIMPLE("NULL => 1", 0, 1);
	//LOCK_TEST_SIMPLE("Invalid => 1", (void*) 0xDEADBEEF, 1);
	LOCK_TEST_SIMPLE("Deleted => 1", &workarea, 1);
	
	BASIC_SCHED_TEST("Zero",
		result = sceKernelTryLockLwMutex_600(&workarea2, 0);
	);
	BASIC_SCHED_TEST("Lock same",
		result = sceKernelTryLockLwMutex_600(&workarea1, 1);
	);
	BASIC_SCHED_TEST("Lock other",
		result = sceKernelTryLockLwMutex_600(&workarea2, 1);
	);

	return 0;
}