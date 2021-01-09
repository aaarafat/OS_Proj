#include <stdlib.h>
struct vecStruct
{
    float *mem;
    int size;
    int last;
};
typedef struct vecStruct vec;
vec *initVec()
{
    vec *pvec = (vec *)malloc(sizeof(vec));
    pvec->last = 0;
    pvec->size = 100;
    pvec->mem = (float *)malloc(pvec->size * sizeof(float));
    return pvec;
}

void vec_delete(vec *pvec)
{
    free(pvec->mem);
    free(pvec);
}

void vec_grow(vec *pvec)
{
    pvec->size *= 2;
    float *mem = (float *)malloc(sizeof(float) * pvec->size);
    memcpy(mem, pvec->mem, pvec->size / 2);
    free(pvec->mem);
    pvec->mem = mem;
}

void vec_push_back(vec *pvec, float number)
{
    if (pvec->last == pvec->size)
    {
        vec_grow(pvec);
    }
    pvec->mem[pvec->last] = number;
    pvec->last++;
}

int vec_length(vec *pvec)
{
    return pvec->last;
}

float vec_get(vec *pvec, int index)
{

    return pvec->mem[index];
}
