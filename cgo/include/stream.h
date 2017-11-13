#ifndef STREAM_H
#define STREAM_H
#include <stdint.h>

extern "C"{
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"

};

#define INT64_MAX  0x7fffffffffffffff

typedef struct StreamInfo{
	char filename[256];
	char url[512];	
}StreamInfo;

typedef struct InputStream {
	AVStream *st;
	AVCodecContext *dec_ctx;
	AVCodec *dec;
	int decoding_needed;

	int64_t start;
	
	int64_t next_dts;
	int64_t dts;

	int64_t next_pts;
	int64_t pts;

	double ts_scale;
	int saw_first_ts;

	uint64_t data_size;
	uint64_t nb_packets;
}InputStream;

typedef struct OutputStream {
	int index;               /* stream index in the output file */
	int encoding_needed;
	AVStream *st;
	int64_t last_mux_dts;
	uint64_t data_size;
	uint64_t packets_written;
	int unavailable;
	int finished;
}OutputStream;

class Stream{
public:
	Stream();
	~Stream();

	int open_input_file(const char* filename);
	int open_output_file(const char* filename);
	int get_input_packet();
	int process_input_packet(InputStream *ist, const AVPacket *pkt);
	int transcode();
	int transcode_init();
	void do_streamcopy(InputStream *ist, OutputStream *ost, const AVPacket *pkt);
	int check_output_constraints(InputStream *ist, OutputStream *ost);
	void write_frame(AVFormatContext *s, AVPacket *pkt, OutputStream *ost);
	int got_eagin();
	void reset_eagin();
	OutputStream* choose_output();
	int seek_to_start();
	int need_output();
	void print_error(const char *filename, int err);
	void close_all_output_streams(OutputStream *ost);
public:
	AVOutputFormat *ofmt;  
	AVFormatContext *ifmt_ctx;
	AVFormatContext *ofmt_ctx;  
	AVPacket pkt;

	int videoindex, audioindex;

	InputStream **input_streams;
	OutputStream **output_streams;

	int nb_input_streams;
	int nb_output_streams;

	bool file_loop;
};

void init();
void deinit();
int create_stream(const char* filename, const char* url);
void close_stream();
int get_stream(const char* url, int inri);
#endif
