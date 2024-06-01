#include <iostream>
#include <string>
#include <chrono>
#include <opencv2\opencv.hpp>
#include <opencv2\core.hpp>
#include <opencv2/objdetect/objdetect.hpp> 
#include <opencv2\highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2\videoio.hpp>

extern "C" {
#include "vc.h"
}

void vc_timer(void) {
	static bool running = false;
	static std::chrono::steady_clock::time_point previousTime = std::chrono::steady_clock::now();

	if (!running) {
		running = true;
	}
	else {
		std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
		std::chrono::steady_clock::duration elapsedTime = currentTime - previousTime;

		std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(elapsedTime);
		double nseconds = time_span.count();

		std::cout << "Tempo decorrido: " << nseconds << "segundos" << std::endl;
	}
}

int main(void) {
	char videofile[100] = "../../video_resistors.mp4";
	cv::VideoCapture capture;
	
	Video video;
	std::string str;
	int key = 0;

	capture.open(videofile);

	if (!capture.isOpened())
	{
		std::cerr << "Erro ao abrir o ficheiro de vídeo!\n";
		return 1;
	}

	video.ntotalframes = (int)capture.get(cv::CAP_PROP_FRAME_COUNT);
	video.fps = (int)capture.get(cv::CAP_PROP_FPS);
	video.width = (int)capture.get(cv::CAP_PROP_FRAME_WIDTH);
	video.height = (int)capture.get(cv::CAP_PROP_FRAME_HEIGHT);

	// Cria uma janela para exibir o vídeo
	cv::namedWindow("VC - VIDEO", cv::WND_PROP_AUTOSIZE);
	cv::namedWindow("VC - MASK", cv::WND_PROP_AUTOSIZE);

	// Inicia o timer
	vc_timer();

	// Criação das imagens IVC - TODO: nomes melhores em vez de 2,3,4,6...
	IVC *image = vc_image_new(video.width, video.height, 3, 255);
	IVC *image_2 = vc_image_new(video.width, video.height, 3, 255);
	IVC *image_3 = vc_image_new(video.width, video.height, 1, 255);
	IVC *image_4 = vc_image_new(video.width, video.height, 1, 255);
	IVC *image_6 = vc_image_new(video.width, video.height, 1, 255);
	IVC *imageBlobPrimeiraCor = vc_image_new(video.width, video.height, 1, 255);
	IVC *imageBlobSegundaCor = vc_image_new(video.width, video.height, 1, 255);
	IVC *imageBlobTerceiraCor = vc_image_new(video.width, video.height, 1, 255);

	// Alocação de memória e criação das imagens para as cores presentes no frame
	ImageColors *img_colors = (ImageColors *)malloc(sizeof(ImageColors));
	vc_initialize_colors(video.width,video.height,img_colors,1,255);

	cv::Mat frame;
	cv::Mat frame2;
	while (key != 'q') {
		// Leitura de uma frame do vídeo
		capture.read(frame);
		capture.read(frame2);

		// Verifica se conseguiu ler a frame
		if (frame.empty()) break;
		
		video.nframe = (int)capture.get(cv::CAP_PROP_POS_FRAMES);

		if(video.nframe < 500) continue;
		// Cópia dados de imagem da estrutura cv::Mat para uma estrutura IVC
		memcpy(image->data, frame.data, video.width * video.height * 3);

		// Executa uma fun��o da nossa biblioteca vc
        vc_bgr_to_rgb(image,image_2);
		if(video.nframe == 660) vc_write_image("../../frame9.pgm", image_2);

        /*Segmentar o corpo*/
		vc_hsv_resistances_segmentation(image_2,image_3,img_colors);

		/*Limpar Ruido das imagens e dilatar*/
        vc_binary_open(image_3,image_3,1,7);

		// damos open só pra imagens mais dificies?
		/* vc_binary_open(img_colors->vermelho,img_colors->vermelho,1,3);
		vc_binary_open(img_colors->verde,img_colors->verde,1,3);
		vc_binary_open(img_colors->laranja,img_colors->laranja,1,3);
		vc_binary_open(img_colors->preto,img_colors->preto,1,3); */

		// Ver se podemos tirar depois
		vc_binary_open(img_colors->azul,img_colors->azul,1,3);
		vc_binary_open(img_colors->castanho,img_colors->castanho,1,3);
		

		int nlabel;
		OVC *blobs = vc_binary_blob_labelling(image_3, image_6, &nlabel);
		if(blobs != NULL)
			vc_binary_blob_info(image_6, blobs, nlabel, 0, false);

		ResistenceColorList ResColors;
		for (int i = 0; i < nlabel; i++){
			// filtrar apenas pelas resistências para o calculo dos ohms
		 	if(blobs[i].area < 6000 || blobs[i].area > 10000){
				continue;
			}

			//ResColors = {0};
			// check para verificar se há resitencias-- correr blob
			bool resitence_check = vc_check_resistence_body(blobs[i].x, blobs[i].y ,blobs[i].width, blobs[i].height, img_colors->corpo);
			if(resitence_check == true){
				ResColors = vc_check_resistence_color(blobs[i].x, blobs[i].y ,blobs[i].width, blobs[i].height, img_colors, video.width);
			}else{
				continue;
			}

			CorContagemImagem cores[] = {
				{1,'0', ResColors.lista_preto, img_colors->preto, INT_MAX},
				{10,'1', ResColors.lista_castanho, img_colors->castanho, INT_MAX},
				{100,'2', ResColors.lista_vermelho, img_colors->vermelho, INT_MAX},
				{1000, '3', ResColors.lista_laranja, img_colors->laranja, INT_MAX},
				//{ResColors.lista_amarelo, img_colors->amarelo, INT_MAX},
				{100000,'5', ResColors.lista_verde, img_colors->verde, INT_MAX},
				{1000000,'6', ResColors.lista_azul, img_colors->azul, INT_MAX}
			};

			CorContagemImagem temp;
			int n = sizeof(cores) / sizeof(cores[0]);
			
			// Ordenar as riscas/cores mais abundantes na resistência 
			for (int x = 0; x < n; ++x) 
			{
				for (int y = x + 1; y < n; ++y) 
				{
					if (cores[x].contagem < cores[y].contagem) 
					{
						temp = cores[x];
						cores[x] = cores[y];
						cores[y] = temp;
					}
				}
			}

			// 2 cores iguais
			if(cores[2].contagem < 200) {
				cores[2] = cores[0];
			}

			// 3 cores iguais
			if(cores[1].contagem < 200) {
				cores[1] = cores[0];
			}

			int nlabelCores;
			OVC *blobsPrimeiraCor = vc_binary_blob_labelling_custom(cores[0].imagem, imageBlobPrimeiraCor, &nlabelCores, blobs[i].x, blobs[i].y, blobs[i].width, blobs[i].height);
			if(blobsPrimeiraCor != NULL)
				vc_binary_blob_info_custom(imageBlobPrimeiraCor, blobsPrimeiraCor, nlabelCores, 300, blobs[i].x, blobs[i].y, blobs[i].width, blobs[i].height);

			OVC *blobsSegundaCor = vc_binary_blob_labelling_custom(cores[1].imagem, imageBlobSegundaCor, &nlabelCores, blobs[i].x, blobs[i].y, blobs[i].width, blobs[i].height);
			if(blobsSegundaCor != NULL)
				vc_binary_blob_info_custom(imageBlobSegundaCor, blobsSegundaCor, nlabelCores, 300, blobs[i].x, blobs[i].y, blobs[i].width, blobs[i].height);

			OVC *blobsTerceiraCor = vc_binary_blob_labelling_custom(cores[2].imagem, imageBlobTerceiraCor, &nlabelCores, blobs[i].x, blobs[i].y, blobs[i].width, blobs[i].height);
			if(blobsTerceiraCor != NULL)
				vc_binary_blob_info_custom(imageBlobTerceiraCor, blobsTerceiraCor, nlabelCores, 300, blobs[i].x, blobs[i].y, blobs[i].width, blobs[i].height);

			// não há blobs
			if(blobsPrimeiraCor == NULL || blobsSegundaCor == NULL || blobsTerceiraCor == NULL) continue;

			if(blobsPrimeiraCor->xc > blobsSegundaCor->xc) {
				swap_cores(&(cores[0]), &(cores[1]));
				swap_blobs(&blobsPrimeiraCor, &blobsSegundaCor);
			} 
			if(blobsSegundaCor->xc > blobsTerceiraCor->xc) {
				swap_cores(&(cores[1]), &(cores[2]));
				swap_blobs(&blobsSegundaCor, &blobsTerceiraCor);
			} 
			if(blobsPrimeiraCor->xc > blobsSegundaCor->xc) {
				swap_cores(&(cores[0]), &(cores[1]));
				swap_blobs(&blobsPrimeiraCor, &blobsSegundaCor);
			}

			cv::Mat imageToShow = cv::Mat(img_colors->vermelho->height, img_colors->vermelho->width, CV_8UC3);
			for (int y = 0; y < img_colors->vermelho->height; y++) {
				for (int x = 0; x < img_colors->vermelho->width; x++) {
					uchar value3 = img_colors->vermelho->data[y * img_colors->vermelho->width + x] == 255 ? 255 : 0;
					uchar value = 0;
					uchar value2 = 0;

					value2 = img_colors->verde->data[y * img_colors->vermelho->width + x] == 255 ? 255 : 0;
					value = img_colors->azul->data[y * img_colors->vermelho->width + x] == 255 ? 255 : 0;

					if(img_colors->castanho->data[y * img_colors->vermelho->width + x] == 255) {
						value = 140;
						value3 = 210;
						value2 = 180;
					}

					else if(img_colors->preto->data[y * img_colors->vermelho->width + x] == 255) {
						value = 211;
						value3 = 211;
						value2 = 211;
					}

					else if(img_colors->laranja->data[y * img_colors->vermelho->width + x] == 255) {
						value = 0;
						value3 = 255;
						value2 = 165;
					}
					
					imageToShow.at<cv::Vec3b>(y, x) = cv::Vec3b(value, value2, value3); // Replicar valor para os três canais
				}
			}
			memcpy(frame2.data, imageToShow.data, video.width * video.height * 3);
			

			/* int nlabelCores;
			OVC *blobsCores[3]; // Array para armazenar os blobs de cada cor

			for (int l = 0; l < 3; l++) {
				IVC *imageBlobCor = vc_image_new(video.width, video.height, 1, 255);
				blobsCores[l] = vc_binary_blob_labelling(cores[l].imagem, imageBlobCor, &nlabelCores);
				
				if (blobsCores[l] != NULL) {
					vc_binary_blob_info(imageBlobCor, blobsCores[l], nlabelCores, true);
				}
			} */

/* 			// Ordena as resistências pelo que está no vídeo ( esquerda para a direita )
			for (int l = 0; l < 3; l++)
			{
				for (int k = l + 1; k < 3; k++)
				{
					if (blobsCores[l]->xc > blobsCores[k]->xc)
					{
						CorContagemImagem corTemp = cores[l];
						OVC *blobTemp = blobsCores[l];
						cores[l] = cores[k];
						cores[k] = corTemp;
						blobsCores[l] = blobsCores[k];
						blobsCores[k] = blobTemp;
					}
				}
			} */

			/* // Encontra o pixel mais à esquerda de cada risca/cor da resistência
			for (int y = blobs[i].y; y < blobs[i].y + blobs[i].height; y++) 
			{
				for (int x = blobs[i].x; x < blobs[i].x + blobs[i].width; x++) 
				{
					int pos = y * image_3->width + x;
					if(cores[0].imagem->data[pos] == 255) {
						if(x < cores[0].xmin) cores[0].xmin = x;
					}
					if(cores[1].imagem->data[pos] == 255) {
						if(x < cores[1].xmin) cores[1].xmin = x;
					}
					if(cores[2].imagem->data[pos] == 255) {
						if(x < cores[2].xmin ) cores[2].xmin = x;
					}
				}
			} */

/* 			if(video.nframe == 340) vc_write_image("../../castanho.pgm", img_colors->castanho);
			if(video.nframe == 340) vc_write_image("../../vermelho.pgm", img_colors->vermelho); */

/* 			// Ordena as resistências pelo que está no vídeo ( esquerda para a direita )
			for (int l = 0; l < 3; l++)
			{
				for (int k = l + 1; k < 3; k++)
				{
					if (cores[l].xmin > cores[k].xmin)
					{
						CorContagemImagem temp = cores[l];
						cores[l] = cores[k];
						cores[k] = temp;
					}
				}
			} */

			char fullString[3];
			fullString[0] = cores[0].digito;
			fullString[1] = cores[1].digito;
			fullString[2] = '\0';
			int valor = atoi(fullString) * cores[2].multiplicador;
			
			str = std::to_string(valor) + "-Ohms";
			int debug = 0;
			if(debug) cv::imshow("VC - MASK", frame2);
			//cv::putText(frame, str, cv::Point(blobs[i].x, blobs[i].y - 5), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 0), 2);
			cv::putText(frame, str, cv::Point(blobs[i].x, blobs[i].y - 5), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(255, 0, 0), 2);

			/*Bounding Box a usar openvc*/
			cv::rectangle(frame, cv::Point(blobs[i].x, blobs[i].y), cv::Point(blobs[i].x + blobs[i].width, blobs[i].y + blobs[i].height ), cv::Scalar(255, 0, 0), 2);
			cv::circle(frame, cv::Point(blobs[i].xc, blobs[i].yc), 1, cv::Scalar(255, 0, 0), 5);
		}
		
		/*Quantidade de blobs detetados*/
		// str = std::string("BLOB'S DETETADOS: ").append(std::to_string(nlabel));
		// cv::putText(frame, str, cv::Point(20, 125), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 2);
		// cv::putText(frame, str, cv::Point(20, 125), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 1);

		str = std::string("RESOLUCAO: ").append(std::to_string(video.width)).append("x").append(std::to_string(video.height));
		cv::putText(frame, str, cv::Point(20, 25), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
		cv::putText(frame, str, cv::Point(20, 25), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);

		str = std::string("TOTAL DE FRAMES: ").append(std::to_string(video.ntotalframes));
		cv::putText(frame, str, cv::Point(20, 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
		cv::putText(frame, str, cv::Point(20, 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);

		str = std::string("FRAME: ").append(std::to_string(video.nframe));
		cv::putText(frame, str, cv::Point(20, 75), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
		cv::putText(frame, str, cv::Point(20, 75), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);

		str = std::string("N. DA FRAME: ").append(std::to_string(video.nframe));
		cv::putText(frame, str, cv::Point(20, 100), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
		cv::putText(frame, str, cv::Point(20, 100), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);

		/* Exibe a frame */
		cv::imshow("VC - VIDEO", frame);
		cv::imshow("VC - MASK", frame2);

		/* Sai da aplica��o, se o utilizador premir a tecla 'q' */
		key = cv::waitKey(1);
	}

    vc_image_free(image);
	vc_image_free(image_2);
	vc_image_free(image_3);
	vc_image_free(image_4);
	vc_image_free(image_6);
    
	if (img_colors != NULL) {
        vc_free_images(img_colors);
        free(img_colors);
    }
	// +++++++++++++++++++++++++

	/* Para o timer e exibe o tempo decorrido */
	vc_timer();

	/* Fecha a janela */
	cv::destroyWindow("VC - VIDEO");
	cv::destroyWindow("VC - MASK");

	/* Fecha o ficheiro de v�deo */
	capture.release();

	return 0;
}