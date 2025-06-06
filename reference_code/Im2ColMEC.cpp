#include <stdio.h>
#include <vector>
#include <math.h>
#include <iostream>
#include <algorithm>
#include <sys/time.h>
#include <omp.h>
#define THREAD_NUM 4

extern"C"
{
    #include<cblas.h>
}
using namespace std;

// MEC
void im2col_mec(float** src, const int &inHeight, const int &intWidth, const int &kHeight,
                const int &kWidth, float* srcIm2col){
    const int outHeight = inHeight - kHeight + 1;
    const int outWidth = intWidth - kWidth + 1;
#pragma omp parallel for num_threads(THREAD_NUM)
    for(int i = 0; i < outWidth; i++){
        int outrow = 0;
        for(int j = 0; j < inHeight; j++){
            for(int k = i; k < i + kWidth; k++){
                srcIm2col[outrow * outWidth + i] = src[j][k];
                outrow++;
            }
        }
    }
}


const int kernel_num = 64;
const int kernel_h = 7;
const int kernel_w = 7;
const int inHeight = 224;
const int inWidth = 224;

int main(){
    // 构造输入矩阵
    float **src = new float*[inHeight];
    for(int i = 0; i < inHeight; i++){
        src[i] = new float[inWidth];
        for(int j = 0; j < inWidth; j++){
            src[i][j] = 0.1;
        }
    }
    // 构造kernel矩阵
    float **kernel[kernel_num];
    for(int i = 0; i < kernel_num; i++){
        kernel[i] = new float*[kernel_h];
        for(int j = 0; j < kernel_h; j++){
            kernel[i][j] = new float[kernel_w];
            for(int k = 0; k < kernel_w; k++){
                kernel[i][j][k] = 0.2;
            }
        }
    }

    // 开始计时
    struct timeval tstart, tend;
    gettimeofday(&tstart, NULL);

    // 对kernel进行Im2col
    float* kernel2col = new float[kernel_num*kernel_h*kernel_w];
    int cnt = 0;
    for(int i = 0; i < kernel_num; i++){
        for(int j = 0; j < kernel_h; j++){
            for(int k = 0; k < kernel_w; k++){
                kernel2col[cnt++] = kernel[i][j][k];
            }
        }
    }
    // 对输入矩阵Im2col
    int outHeight = inHeight - kernel_h + 1;
    int outWidth = inWidth - kernel_w + 1;
    float *srcIm2col = new float[outWidth * inHeight * kernel_w];
    im2col_mec(src, inHeight, inWidth, kernel_h, kernel_w, srcIm2col);

    // 使用Blas库实现矩阵乘法
    float **output = new float*[outHeight];

#pragma omp parallel for num_threads(THREAD_NUM)
    for(int i = 0; i < outHeight; i++){
        output[i] = new float [kernel_num * outWidth];
        cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,kernel_num,
            outWidth, kernel_w * kernel_h,1,
            kernel2col, kernel_h * kernel_w,
            srcIm2col + i * outWidth, outWidth, 0, output[i], outWidth);
    }

    // 结束计时
    gettimeofday(&tend, NULL);
    cout<<"MEC Total time cost: "<<(tend.tv_sec-tstart.tv_sec)*1000 + (tend.tv_usec-tstart.tv_usec)/1000<<" ms"<<endl;

    //
    delete [] kernel2col;
    delete [] srcIm2col;

    for(int i = 0; i < outHeight; i++){
        delete [] output[i];
    }

    for(int i = 0; i < kernel_num; i++){
        for(int j = 0; j < kernel_h; j++){
            delete [] kernel[i][j];
        }
        delete [] kernel[i];
    }

    for(int i = 0; i < inHeight; i++){
        delete [] src[i];
    }

    return 0;
}