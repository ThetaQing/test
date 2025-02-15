#include <opencv2/core/core.hpp>
#include<opencv2/highgui/highgui.hpp>  
#include<opencv2/imgproc/imgproc.hpp>  
#include <iostream>  

using namespace cv;
using namespace std;

Mat getFFTresultImg(Mat& completeI, CvSize srcSize)
{
	Mat planes[2];
	split(completeI, planes);//把变换后的结果分割到各个数组的两页中，方便后续操作 
	Mat magI;
	magnitude(planes[0], planes[1], magI);//求傅里叶变换各频率的幅值，幅值放在第一页中。  


	//傅立叶变换的幅度值范围大到不适合在屏幕上显示。高值在屏幕上显示为白点，  
	//而低值为黑点，高低值的变化无法有效分辨。为了在屏幕上凸显出高低变化的连续性，我们可以用对数尺度来替换线性尺度:  
	magI += 1;
	log(magI, magI);//取对数  
	magI = magI(Rect(0, 0, srcSize.width, srcSize.height));//前边对原始图像进行了扩展，这里把对原始图像傅里叶变换取出，剔除扩展部分。  


	//这一步的目的仍然是为了显示。 现在我们有了重分布后的幅度图，  
	//但是幅度值仍然超过可显示范围[0,1] 。我们使用 normalize() 函数将幅度归一化到可显示范围。  
	normalize(magI, magI, 0, 1, CV_MINMAX);//傅里叶图像进行归一化。  


	//重新分配象限，使（0,0）移动到图像中心，  
	//在《数字图像处理》中，傅里叶变换之前要对源图像乘以（-1）^(x+y)进行中心化。  
	//这是是对傅里叶变换结果进行中心化  
	int cx = magI.cols / 2;
	int cy = magI.rows / 2;

	Mat tmp;
	Mat q0(magI, Rect(0, 0, cx, cy));
	Mat q1(magI, Rect(cx, 0, cx, cy));
	Mat q2(magI, Rect(0, cy, cx, cy));
	Mat q3(magI, Rect(cx, cy, cx, cy));


	q0.copyTo(tmp);
	q3.copyTo(q0);
	tmp.copyTo(q3);

	q1.copyTo(tmp);
	q2.copyTo(q1);
	tmp.copyTo(q2);
	return magI;
}

Mat FFT(Mat& src_gray)
{
	//Mat src_gray;
	//cvtColor(src, src_gray, CV_RGB2GRAY);//灰度图像做傅里叶变换  

	int m = getOptimalDFTSize(src_gray.rows);//2,3,5的倍数有更高效率的傅里叶转换  
	int n = getOptimalDFTSize(src_gray.cols);

	Mat dst;
	///把灰度图像放在左上角，在右边和下边扩展图像，扩展部分填充为0；  
	// 0, m - src_gray.rows, 0, n - src_gray.cols 上边填充0行，下面填充m - src_gray.rows行
	copyMakeBorder(src_gray, dst, 0, m - src_gray.rows, 0, n - src_gray.cols, BORDER_CONSTANT, Scalar::all(0));
	//cout << dst.size() << endl;

	//新建一个两页的array，其中第一页用扩展后的图像初始化，第二页初始化为0  
	Mat planes[] = { Mat_<float>(dst), Mat::zeros(dst.size(), CV_32F) };
	Mat  completeI;
	merge(planes, 2, completeI);//把两页合成一个2通道的mat  

	//对上边合成的mat进行傅里叶变换，支持原地操作，傅里叶变换结果为复数。通道1存的是实部，通道2存的是虚部。  
	dft(completeI, completeI);


	return completeI.clone();
}
//计算高斯滤波系数矩阵
Mat clcGLPFMat(Mat& mat, int D0)
{
	int width = mat.rows;
	int height = mat.cols;
	int M = width;
	int N = height;
	Mat mat_GLPF(mat.size(), CV_32FC1);

	Mat U, V;
	U.create(M, N, CV_32FC1);
	V.create(M, N, CV_32FC1);

	for (int u = 0; u < M; ++u)
	{
		for (int v = 0; v < N; ++v)
		{
			float tm1, tm2;
			tm1 = (float)((u > cvRound(M / 2)) ? u - M : u);
			tm2 = (float)((v > cvRound(N / 2)) ? v - N : v);

			U.at<float>(u, v) = tm1;
			V.at<float>(u, v) = tm2;
		}
	}


	for (int u = 0; u < M; ++u)
	{
		for (int v = 0; v < N; ++v)
		{
			float t1, t2;
			t1 = U.at<float>(u, v);
			t2 = V.at<float>(u, v);
			float Elem_D = t1 * t1 + t2 * t2;
			mat_GLPF.at<float>(u, v) = (float)(exp(-(Elem_D) / (2 * D0 * D0)) / 2 / 3.1415 / (2 * D0 * D0));
		}
	}
	Mat_<float>::iterator begainIt = mat_GLPF.begin<float>();
	Mat_<float>::iterator endIt = mat_GLPF.end<float>();
	float sumValue = 0;
	for (; begainIt != endIt; begainIt++)
	{
		sumValue += *begainIt;
	}
	mat_GLPF = mat_GLPF / sumValue;
	return mat_GLPF.clone();
}

Mat mask(Mat& plane)
{
	Mat FFTresult = FFT(plane);//傅里叶变换包含实部和虚部，分别放在两个planes里
	Mat planes[2];
	split(FFTresult, planes);
	imshow("FFTresult", getFFTresultImg(FFTresult, FFTresult.size()));

	Mat GLPFMatIM = clcGLPFMat(planes[0], 10);//高斯滤波系数
	Mat GLPFMatRE = clcGLPFMat(planes[1], 10);
	planes[0] = GLPFMatIM.mul(planes[0]);
	planes[1] = GLPFMatRE.mul(planes[1]);
	Mat GLPFresult;
	merge(planes, 2, GLPFresult);       //实部虚部分别高斯滤波，然后合成到滤波结果
	imshow("FFTresultAfterFlit", getFFTresultImg(GLPFresult, GLPFresult.size()));

	Mat maskResult;
	dft(GLPFresult, maskResult, DFT_INVERSE + DFT_SCALE);//滤波结果做傅里叶反变换

	split(maskResult, planes);//把反变换后的结果分割到两页中，方便后续操作 
	Mat mask;
	magnitude(planes[0], planes[1], mask);//求傅里叶变换各频率的幅值  
	return mask.clone();
}


