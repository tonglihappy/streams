/*================================================================
*   Copyright (C) 2017 KingSoft Cloud Ltd. All rights reserved.
*   
*   文件名称：stream.cpp
*   创 建 者：tongli
*   创建日期：2017年9月15日
*   描    述：
*
================================================================*/
#include <stdio.h>
#include <iostream>
#include <pthread.h>
#include <map>
#define __STDC_CONSTANT_MACROS

#include "stream.h"
#include "utils.h"

extern "C"{
#include "libavformat/avformat.h"
#include "libavutil/mathematics.h"
#include "libavutil/time.h"
#include "libavutil/avstring.h"
#include "libavutil/avutil.h"
}

int recv_exit = 0;
int inited = 0;
int stream_count = 0;
//std::map<int, Stream*> streams;

static AVRational _AVRational(int num, int den) {
	AVRational r = {num, den};
	return r;
}

void init(){
	if(!inited){
		av_log_set_level(AV_LOG_QUIET);
		av_register_all();  
		avformat_network_init(); 
		inited = 1;
	}
}

void deinit(){
	avformat_network_deinit();
}

Stream::Stream(){
	ifmt_ctx = ofmt_ctx = NULL;
 	videoindex = audioindex = -1;
	nb_input_streams = 0;
	nb_output_streams = 0;
	input_streams = NULL;
	output_streams = NULL;
	file_loop = false;
}

Stream::~Stream(){
	int i;
	avformat_close_input(&ifmt_ctx);
	if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))  
		avio_close(ofmt_ctx->pb);  
	avformat_free_context(ofmt_ctx); 

	for(i = 0; i < nb_input_streams; i++){
		InputStream *ist = input_streams[i];
		avcodec_close(ist->dec_ctx);
		avcodec_free_context(&ist->dec_ctx);
		av_freep(&input_streams[i]);
	}

	for(i = 0; i < nb_output_streams; i++){
		av_freep(&output_streams[i]);
	}

	av_freep(&input_streams);
	av_freep(&output_streams);
	std::cout << "free end" << std::endl;
}

void Stream::print_error(const char *filename, int err){
	char errbuf[128];
	const char *errbuf_ptr = errbuf;

	if (av_strerror(err, errbuf, sizeof(errbuf)) < 0)
			errbuf_ptr = strerror(AVUNERROR(err));
	av_log(NULL, AV_LOG_ERROR, "%s: %s\n", filename, errbuf_ptr);
}

int Stream::open_input_file(const char* filename){
	int ret;
	unsigned int i;
	
	ifmt_ctx = avformat_alloc_context();
	if (!ifmt_ctx) {
		std::cout << "ifmt_ctx init failed" << std::endl;
	}
	
	if((ret = avformat_open_input(&ifmt_ctx, filename, 0, 0)) < 0) {  
		std::cout << "Could not open input file." << std::endl;
		return ret;
	}  

	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {  
		std::cout << "Failed to retrieve input stream information" << std::endl;  
		return ret; 
	}  
	
	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		AVStream *st = ifmt_ctx->streams[i];
		AVCodecParameters *par = st->codecpar;
		InputStream *ist = (InputStream*)av_mallocz(sizeof(*ist));
		if(ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){  
			videoindex = i;  
		}else if(ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
			audioindex = i;
		}

		input_streams = (InputStream**)grow_array(input_streams, sizeof(*input_streams),&nb_input_streams, nb_input_streams+1);
		input_streams[nb_input_streams-1] = ist;

		ist->st = st;
		ist->ts_scale = 1.0;
		ist->dec = avcodec_find_decoder(st->codecpar->codec_id);

		ist->dec_ctx = avcodec_alloc_context3(ist->dec);
		
		if (!ist->dec_ctx) {
			av_log(NULL, AV_LOG_ERROR, "Error allocating the decoder context.\n");
			return -1;
		}
		ret = avcodec_parameters_to_context(ist->dec_ctx, par);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Error initializing the decoder context.\n");
			return -1;
		}

		ret = avcodec_parameters_from_context(par, ist->dec_ctx);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Error initializing the decoder context.\n");
			return -1;
		}

		//av_dump_format(ifmt_ctx, 0, filename, 0);
	}

	return 0;
}

int Stream::open_output_file(const char* filename){
	int ret;
	unsigned int i;
	ofmt_ctx = avformat_alloc_context();
	avformat_alloc_output_context2(&ofmt_ctx, NULL, "flv", filename);
	if (!ofmt_ctx) {  
		av_log(NULL, AV_LOG_ERROR, "init ofmt failed\n");
		ret = AVERROR_UNKNOWN;  
		return ret;  
	}
	ofmt = ofmt_ctx->oformat;
	
	for (i = 0; i < ifmt_ctx->nb_streams; i++) { 
		OutputStream *ost;
		AVStream *in_stream = ifmt_ctx->streams[i]; 
		AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
		if (!out_stream) {  
			std::cout << "Failed allocating output stream" << std::endl;  
			ret = AVERROR_UNKNOWN;  
			return ret;
		}  

		if (!(ost = (OutputStream*)av_mallocz(sizeof(*ost))))
			return -1;

		output_streams = (OutputStream**)grow_array(output_streams, sizeof(*output_streams),&nb_output_streams, nb_output_streams+1);
		output_streams[nb_output_streams - 1] = ost;
		out_stream->cur_dts = 0;
		ost->st = out_stream;
		ost->index = i;

		ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
		if (ret < 0) {  
			std::cout << "Failed to copy context from input to output stream codec context" << std::endl;  
			return ret;  
		}  
		
		//out_stream->codec->codec_tag = 0;  
		//if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)  
			//out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER; 
	}

	if (!(ofmt->flags & AVFMT_NOFILE)) {  
		ret = avio_open2(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE, &ofmt_ctx->interrupt_callback,
			NULL);
		if (ret < 0) {  
			std::cout << "Could not open output " << filename << std::endl;  
			return ret; 
		}  
	}

	return 0;
}

int Stream::get_input_packet(){
	unsigned int i;
	for (i = 0; i < ifmt_ctx->nb_streams; i++){
		InputStream *ist = input_streams[i];
		int64_t pts = av_rescale(ist->dts, 1000000, AV_TIME_BASE);
		int64_t now = av_gettime_relative() - ist->start;
		
		if(pts > now)
			return AVERROR(EAGAIN);
	}

	return av_read_frame(ifmt_ctx, &pkt);;
}

void Stream::close_all_output_streams(OutputStream *ost){
	int i;
 	for(i = 0; i < nb_output_streams; i++){
		output_streams[i]->finished = 1;
	}		
}

void Stream::write_frame(AVFormatContext *s, AVPacket *pkt, OutputStream *ost){
	int ret;
	//AVCodecContext *avctx = ost->st->codec;

	ost->last_mux_dts = pkt->dts;

	ost->data_size += pkt->size;
	ost->packets_written++;

	pkt->stream_index = ost->index;
	ret = av_interleaved_write_frame(s, pkt);
	if (ret < 0) {
		print_error("av_interleaved_write_frame()", ret);
		close_all_output_streams(ost);
	}
	av_packet_unref(pkt);
}

void Stream::do_streamcopy(InputStream *ist, OutputStream *ost, const AVPacket *pkt){
	AVPacket opkt;

	av_init_packet(&opkt);

	if (pkt->pts != AV_NOPTS_VALUE)
		opkt.pts = av_rescale_q(pkt->pts, ist->st->time_base, ost->st->time_base);
	else
		opkt.pts = AV_NOPTS_VALUE;

	if (pkt->dts == AV_NOPTS_VALUE)
		opkt.dts = av_rescale_q(ist->dts, AV_TIME_BASE_Q, ost->st->time_base);
	else
		opkt.dts = av_rescale_q(pkt->dts, ist->st->time_base, ost->st->time_base);
	
	opkt.duration = av_rescale_q(pkt->duration, ist->st->time_base, ost->st->time_base);
	opkt.flags    = pkt->flags;

	opkt.data = pkt->data;
	opkt.size = pkt->size;
	
	av_copy_packet_side_data(&opkt, pkt);
	write_frame(ofmt_ctx, &opkt, ost);
}

int Stream::check_output_constraints(InputStream *ist, OutputStream *ost){
	int ist_index = ist->st->index;
	
	if(ost->index != ist_index)
		return 0;

	return 1;
}

int Stream::process_input_packet(InputStream *ist, const AVPacket *pkt){
	int ret = 0, i;
	int got_output = 0;
	
	AVPacket avpkt;
	if (!ist->saw_first_ts) {
		ist->dts = (int64_t)(ist->st->avg_frame_rate.num ? - ist->dec_ctx->has_b_frames * AV_TIME_BASE / av_q2d(ist->st->avg_frame_rate) : 0);
		ist->pts = 0;
		if (pkt && pkt->pts != AV_NOPTS_VALUE && !ist->decoding_needed) {
			ist->dts += av_rescale_q(pkt->pts, ist->st->time_base, AV_TIME_BASE_Q);
			ist->pts = ist->dts; //unused but better to set it to a value thats not totally wrong
		}
		ist->saw_first_ts = 1;
	}

	if (ist->next_dts == AV_NOPTS_VALUE)
		ist->next_dts = ist->dts;
	if (ist->next_pts == AV_NOPTS_VALUE)
		ist->next_pts = ist->pts;

	if (!pkt) {
		/* EOF handling */
		av_init_packet(&avpkt);
		avpkt.data = NULL;
		avpkt.size = 0;
		goto handle_eof;
	} else {
		avpkt = *pkt;
	}

	if (pkt && pkt->dts != AV_NOPTS_VALUE) {
		ist->next_dts = ist->dts = av_rescale_q(pkt->dts, ist->st->time_base, AV_TIME_BASE_Q);
		if (ist->dec_ctx->codec_type != AVMEDIA_TYPE_VIDEO || !ist->decoding_needed)
			ist->next_pts = ist->pts = ist->dts;
	}
	if(ist->decoding_needed){
handle_eof:
		ist->pts = ist->next_pts;
		ist->dts = ist->next_dts;
	}

 	if (!ist->decoding_needed) {
		ist->dts = ist->next_dts;
		switch (ist->dec_ctx->codec_type) {
		case AVMEDIA_TYPE_AUDIO:
			ist->next_dts += ((int64_t)AV_TIME_BASE * ist->dec_ctx->frame_size) /
				ist->dec_ctx->sample_rate;
			
			break;
		case AVMEDIA_TYPE_VIDEO:
			if (pkt->duration) {
				ist->next_dts += av_rescale_q(pkt->duration, ist->st->time_base, AV_TIME_BASE_Q);
			} else if(ist->dec_ctx->framerate.num != 0) {
				int ticks = av_stream_get_parser(ist->st) ? av_stream_get_parser(ist->st)->repeat_pict + 1 : ist->dec_ctx->ticks_per_frame;
				ist->next_dts += ((int64_t)AV_TIME_BASE *
					ist->dec_ctx->framerate.den * ticks) /
					ist->dec_ctx->framerate.num / ist->dec_ctx->ticks_per_frame;
			}
			
			break;

			ist->pts = ist->dts;
			ist->next_pts = ist->next_dts;
		}
		
		for (i = 0; pkt && i < nb_input_streams; i++) {
			OutputStream *ost = output_streams[i];
			if(!check_output_constraints(ist,ost))
				continue;

			do_streamcopy(ist, ost, pkt);
		}
	}

	return 0;
}

OutputStream* Stream::choose_output(){
	int i;
	int64_t opts_min = INT64_MAX;
	OutputStream *ost_min = NULL;

	for(i = 0; i < nb_output_streams; i++){
		OutputStream *ost = output_streams[i];
		int64_t opts = av_rescale_q(ost->st->cur_dts, ost->st->time_base, AV_TIME_BASE_Q);

		if(!ost->unavailable && opts < opts_min){
			opts_min = opts;
			ost_min = ost;
		}
	}

	return ost_min;
}

int Stream::got_eagin(){
	int i;
	for(i = 0; i < nb_output_streams; i++){
		if(output_streams[i]->unavailable)
			return 1;
	}

	return 0;
}

void Stream::reset_eagin(){
	int i;
	for(i = 0; i < nb_output_streams; i++){
		output_streams[i]->unavailable = 0;
	}
}

int Stream::seek_to_start(){
	InputStream *ist;
	AVCodecContext *avctx;
	unsigned int i;
	int ret;
	int64_t duration = 0;

	ret = av_seek_frame(ifmt_ctx, -1, ifmt_ctx->start_time, 0);
	if(ret < 0)
		return ret;

	for(i = 0; i < nb_input_streams; i++){
		ist = input_streams[i];
		avctx = ist->dec_ctx;

		if(ist->st->avg_frame_rate.num){
			duration = av_rescale_q(1, ist->st->avg_frame_rate, ist->st->time_base);
		}else
			duration = 1;
	}


	return ret;
}

int Stream::transcode_init(){
	int ret = 0;
	unsigned int i;
	int want_sdp = 1;

	for(i = 0; i < ifmt_ctx->nb_streams; i++){
		input_streams[i]->start = av_gettime_relative();
	}

	for(i = 0; i < ofmt_ctx->nb_streams; i++){
		if (!ofmt_ctx->nb_streams && !(ofmt_ctx->oformat->flags & AVFMT_NOSTREAMS)) {
			av_log(NULL, AV_LOG_ERROR, "Output file #%d does not contain any stream\n", i);
			return AVERROR(EINVAL);
		}
	}

	ret = avformat_write_header(ofmt_ctx, NULL);  
	if (ret < 0) {  
		av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output URL");
		return ret;  
	}  

	return 0;
}

int Stream::need_output(){
	unsigned int i;
	for(i = 0; i < nb_output_streams; i++){
		OutputStream *ost = output_streams[i];
		if(ost->finished){			
			return 0;	
		}
	}
	return 1;
}

int Stream::transcode(){
	int ret;
	unsigned int i;
	InputStream *ist;
	OutputStream *ost;
	int64_t duration;
	AVRational bq = {1,1};
	
	transcode_init();
	
	while(!recv_exit){  
		ost = choose_output();
		if(!ost){
			if(got_eagin()){
				reset_eagin();
				av_usleep(10000);
				continue;
			}
		}
		ret = get_input_packet();
		
		if(pkt.dts < 0)
			pkt.dts = 0;
		
		if (ret == AVERROR(EAGAIN)) {
			ost->unavailable = 1;
			continue;
		}
		
		if(ret < 0 && file_loop){
			if(ret = seek_to_start() < 0){

			}
		}

		if(ret < 0){
			if(ret != AVERROR_EOF){
				return ret;
			}else
				break;

			for(i = 0; i < nb_input_streams; i++){
				ist = input_streams[i];
				ret = process_input_packet(ist, NULL);
				if (ret > 0)
					return 0;
			}
		}
		
		if(!need_output()){
			break;	
		}
		reset_eagin();

		ist = input_streams[pkt.stream_index];
		
		ist->data_size += pkt.size;
		ist->nb_packets++;
		
		if (ist->nb_packets == 1){
			for (i = 0; i < ist->st->nb_side_data; i++) {
				AVPacketSideData *src_sd = &ist->st->side_data[i];
				uint8_t *dst_data;

				if(src_sd->type == AV_PKT_DATA_DISPLAYMATRIX)
					continue;
					
				if (av_packet_get_side_data(&pkt, src_sd->type, NULL))
					continue;

				dst_data = av_packet_new_side_data(&pkt, src_sd->type, src_sd->size);
				if (!dst_data)
					return -1;

				memcpy(dst_data, src_sd->data, src_sd->size);
			}
		}
		
		if (pkt.dts != AV_NOPTS_VALUE)
			pkt.dts += av_rescale_q(0, AV_TIME_BASE_Q, ist->st->time_base);
		if (pkt.pts != AV_NOPTS_VALUE)
			pkt.pts += av_rescale_q(0, AV_TIME_BASE_Q, ist->st->time_base);

		if (pkt.pts != AV_NOPTS_VALUE)
			pkt.pts *= (int64_t)ist->ts_scale;
		if (pkt.dts != AV_NOPTS_VALUE)
			pkt.dts *= (int64_t)ist->ts_scale;
		
		duration = av_rescale_q(0, bq, ist->st->time_base);
		if(pkt.pts != AV_NOPTS_VALUE){
			pkt.pts += duration;
		}

		if(pkt.dts != AV_NOPTS_VALUE){
			pkt.dts += duration;
		}
		
		ret = process_input_packet(ist, &pkt);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "process intput packets failed");
            return ret;
		}
	}
	
    av_write_trailer(ofmt_ctx);
    
	av_packet_unref(&pkt);

	return ret;
}

int create_stream(const char* filename, const char* url){
	int ret;

	//stream_count++;
	Stream *stream = new Stream();
	//streams.insert(std::pair<int, Stream*>(stream_count, stream));

	if(ret = stream->open_input_file(filename) < 0){
		std::cout << "open input file failed" << std::endl;
		goto end;
	}

	if(ret = stream->open_output_file(url) < 0){
		std::cout << "open output file failed" << std::endl;
		goto end;
	}
	if (ret = stream->transcode() < 0){
		goto end;
	}

end:
	std::cout << "delete stream success" << std::endl;
	delete stream;
	stream = NULL;
	recv_exit = 0;
	return ret; 
}

void close_stream(){
	recv_exit = 1;
	inited = 0;
}

int get_stream(const char* url, int infi){
	int ret;
	int nb_packets = 0;
	int64_t time;
	AVPacket pkt;
	AVFormatContext *ifmt_ctx = NULL;
	
	if(!inited){
		av_register_all(); 
		avformat_network_init();
	}

	if (!(ifmt_ctx = avformat_alloc_context()))
		goto end;

	if ((ret = avformat_open_input(&ifmt_ctx, url, 0, NULL)) < 0) {
		goto end;
	}

	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) { 
		goto end;
	}
	
	av_dump_format(ifmt_ctx, 0, url, 0);
	while(1){
		ret = av_read_frame(ifmt_ctx, &pkt);
		if(ret < 0)
			goto end;
		if(nb_packets++ == 10 && infi == 0){
			avformat_close_input(&ifmt_ctx);
			avformat_network_deinit();
			return 200;
		}

	}
end:
	avformat_close_input(&ifmt_ctx);
	avformat_network_deinit();
	return -1;
}

void* thread_func(void *arg)
{  
   	StreamInfo *si = (StreamInfo*)arg;

	/*	
	av_register_all();  
	avformat_network_init(); 
	*/
	Stream *stream = new Stream();
	if(stream->open_input_file(si->filename) < 0){
		std::cout << "open input file failed" << std::endl;
		goto end;
	}

	if(stream->open_output_file(si->url) < 0){
		std::cout << "open output file failed" << std::endl;
		goto end;
	}

	 if (stream->transcode() < 0){
		goto end;
	 }

end:
	//avformat_network_deinit();
	delete stream;
	stream = NULL;

	return 0;  
}

int test_multi_threads(){
	int ret;
	av_register_all();  
	avformat_network_init(); 
	char* in_filename = "./ss.flv";
	char* out_filename = "rtmp://112.84.131.164/videoqa.uplive.ks-cdn.com/live/test011";
	char* out_filename1 = "rtmp://112.84.131.164/videoqa.uplive.ks-cdn.com/live/test012";
	pthread_t id_1,id_2;
	StreamInfo *si = (StreamInfo *)av_malloc(sizeof(StreamInfo));
	memcpy(si->filename, in_filename, strlen(in_filename));
	memcpy(si->url, out_filename, strlen(out_filename));
	
	StreamInfo *si1 = (StreamInfo *)av_malloc(sizeof(StreamInfo));
	memcpy(si1->filename, in_filename, strlen(in_filename));
	memcpy(si1->url, out_filename1, strlen(out_filename1));
	
	ret = pthread_create(&id_1, NULL, &thread_func, si);
	if(ret != 0){
		std::cout << "create thread 1 failed" << std::endl;
	}

	ret = pthread_create(&id_2, NULL, &thread_func, si1);
	if(ret != 0){
		std::cout << "create thread 2 failed" << std::endl;
	}

	pthread_join(id_1, NULL); 
	avformat_network_deinit();
	return 0;
}
