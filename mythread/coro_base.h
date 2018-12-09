#ifndef COROXX_CORO_BASE_H_
#define COROXX_CORO_BASE_H_

#include <stdint.h>
#include <stdlib.h>
#include <functional>
#include "coro.h"
#include "mythread.h"

namespace mythread {
typedef void CoroutineFunc(void* data);
typedef std::function<void(void)> MythreadFunction;
typedef void CoroDataDestructor(void* obj);
template<typename T>
void StandardCoroDataDestructor(void* obj) {
	if (NULL != obj) {
		T* v = (T*) obj;
		delete v;
	}
}

struct CoroutineDataContext {
	MythreadFunction* func;
	void* data;
	void* coro;
	CoroutineDataContext() :
		func(NULL),data(NULL), coro(NULL) {
	}
};

}

#endif /* COROXX_CORO_BASE_H_ */
