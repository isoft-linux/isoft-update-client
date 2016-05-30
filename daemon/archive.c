#include "archive.h"

extern char *strlcpy(char *dest, const char *src, int size);

int tar_verbose = 0;
int tar_use_gnu = 0;
int xz_verbose = 0;
int archive_verbose = 0;

static int tar_create(const char *tarfile, const char *rootdir, libtar_list_t *l)
{
	TAR *t;
	char *pathname;
	char buf[MAXPATHLEN];
	libtar_listptr_t lp;

	if (tar_open(&t, (char *)tarfile,
		     NULL,
		     O_WRONLY | O_CREAT, 0644,
		     (tar_verbose ? TAR_VERBOSE : 0)
		     | (tar_use_gnu ? TAR_GNU : 0)) == -1)
	{
		fprintf(stderr, "tar_open(): %s\n", strerror(errno));
		return -1;
	}

	libtar_listptr_reset(&lp);
	while (libtar_list_next(l, &lp) != 0)
	{
		pathname = (char *)libtar_listptr_data(&lp);
		if (pathname[0] != '/' && rootdir != NULL)
			snprintf(buf, sizeof(buf), "%s/%s", rootdir, pathname);
		else
			strlcpy(buf, pathname, sizeof(buf));
		if (tar_append_tree(t, buf, pathname) != 0)
		{
			fprintf(stderr,
				"tar_append_tree(\"%s\", \"%s\"): %s\n", buf,
				pathname, strerror(errno));
			tar_close(t);
			return -1;
		}
	}

	if (tar_append_eof(t) != 0)
	{
		fprintf(stderr, "tar_append_eof(): %s\n", strerror(errno));
		tar_close(t);
		return -1;
	}

	if (tar_close(t) != 0)
	{
		fprintf(stderr, "tar_close(): %s\n", strerror(errno));
		return -1;
	}

	return 0;
}


static int tar_extract(const char *tarfile, const char *rootdir)
{
	TAR *t;

	if (tar_open(&t, (char *)tarfile,
		     NULL,
		     O_RDONLY, 0,
		     (tar_verbose ? TAR_VERBOSE : 0)
		     | (tar_use_gnu ? TAR_GNU : 0)) == -1)
	{
		fprintf(stderr, "tar_open(): %s\n", strerror(errno));
		return -1;
	}

	if (tar_extract_all(t, (char *)rootdir) != 0)
	{
		fprintf(stderr, "tar_extract_all(): %s\n", strerror(errno));
		return -1;
	}

	if (tar_close(t) != 0)
	{
		fprintf(stderr, "tar_close(): %s\n", strerror(errno));
		return -1;
	}

	return 0;
}


static int tar_compress(const char *infile, const char *tarfile)
{
	char *rootdir = NULL;
	libtar_list_t *l;

	l = libtar_list_new(LIST_QUEUE, NULL);
	libtar_list_add(l, (void *)infile);
	return tar_create(tarfile, rootdir, l);
}

int tar_decompress(const char *tarfile, const char *outdir)
{
	return tar_extract(tarfile, outdir);	
}

/* ----------------- about xz ------------------ */

static bool xz_init_encoder(lzma_stream *strm, uint32_t preset)
{
	lzma_ret ret = lzma_easy_encoder(strm, preset, LZMA_CHECK_CRC64);
	if (ret == LZMA_OK)
		return true;

	const char *msg;
	switch (ret) {
	case LZMA_MEM_ERROR:
		msg = "Memory allocation failed";
		break;

	case LZMA_OPTIONS_ERROR:
		msg = "Specified preset is not supported";
		break;

	case LZMA_UNSUPPORTED_CHECK:
		msg = "Specified integrity check is not supported";
		break;

	default:
		msg = "Unknown error, possibly a bug";
		break;
	}

	fprintf(stderr, "Error initializing the encoder: %s (error code %u)\n",
			msg, ret);
	return false;
}


static bool xz_init_decoder(lzma_stream *strm)
{
	lzma_ret ret = lzma_stream_decoder(
			strm, UINT64_MAX, LZMA_CONCATENATED);

	if (ret == LZMA_OK)
		return true;

	const char *msg;
	switch (ret) {
	case LZMA_MEM_ERROR:
		msg = "Memory allocation failed";
		break;

	case LZMA_OPTIONS_ERROR:
		msg = "Unsupported decompressor flags";
		break;

	default:
		msg = "Unknown error, possibly a bug";
		break;
	}

	fprintf(stderr, "Error initializing the decoder: %s (error code %u)\n",
			msg, ret);
	return false;
}


static bool xz_start_decode(lzma_stream *strm, const char *inname, FILE *infile, FILE *outfile)
{
	lzma_action action = LZMA_RUN;

	uint8_t inbuf[BUFSIZ];
	uint8_t outbuf[BUFSIZ];
	int fsize, fcur;

	fseek(infile, 0, SEEK_END);
	fsize = ftell(infile);
	rewind(infile);

	strm->next_in = NULL;
	strm->avail_in = 0;
	strm->next_out = outbuf;
	strm->avail_out = sizeof(outbuf);

	while (true) {
		if (strm->avail_in == 0 && !feof(infile)) {
			strm->next_in = inbuf;
			strm->avail_in = fread(inbuf, 1, sizeof(inbuf),
					infile);

			if (xz_verbose) {
				fcur = ftell(infile);
				printf("decompressing archive use Xz ... ... ");
				printf("\e[1;33m[%.2f%%\e[0m]\r", fcur / (float)fsize * 100);
				fflush(stdout);
			}

			if (ferror(infile)) {
				fprintf(stderr, "%s: Read error: %s\n",
						inname, strerror(errno));
				return false;
			}
			if (feof(infile))
				action = LZMA_FINISH;
		}

		lzma_ret ret = lzma_code(strm, action);

		if (strm->avail_out == 0 || ret == LZMA_STREAM_END) {
			size_t write_size = sizeof(outbuf) - strm->avail_out;

			if (fwrite(outbuf, 1, write_size, outfile)
					!= write_size) {
				fprintf(stderr, "Write error: %s\n",
						strerror(errno));
				return false;
			}

			strm->next_out = outbuf;
			strm->avail_out = sizeof(outbuf);
		}

		if (ret != LZMA_OK) {
			if (ret == LZMA_STREAM_END)
				return true;

			const char *msg;
			switch (ret) {
			case LZMA_MEM_ERROR:
				msg = "Memory allocation failed";
				break;

			case LZMA_FORMAT_ERROR:
				// .xz magic bytes weren't found.
				msg = "The input is not in the .xz format";
				break;

			case LZMA_OPTIONS_ERROR:
				msg = "Unsupported compression options";
				break;

			case LZMA_DATA_ERROR:
				msg = "Compressed file is corrupt";
				break;

			case LZMA_BUF_ERROR:
				msg = "Compressed file is truncated or "
					"otherwise corrupt";
				break;

			default:
				// This is most likely LZMA_PROG_ERROR.
				msg = "Unknown error, possibly a bug";
				break;
			}

			fprintf(stderr, "%s: Decoder error: "
					"%s (error code %u)\n",
					inname, msg, ret);
			return false;
		}
	}
	return true;
}


static bool xz_start_encode(lzma_stream *strm, FILE *infile, FILE *outfile)
{
	lzma_action action = LZMA_RUN;

	uint8_t inbuf[BUFSIZ];
	uint8_t outbuf[BUFSIZ];
	int fsize, fcur;

	fseek(infile, 0, SEEK_END);
	fsize = ftell(infile);
	rewind(infile);

	strm->next_in = NULL;
	strm->avail_in = 0;
	strm->next_out = outbuf;
	strm->avail_out = sizeof(outbuf);

	while (true) {
		// Fill the input buffer if it is empty.
		if (strm->avail_in == 0 && !feof(infile)) {
			strm->next_in = inbuf;
			strm->avail_in = fread(inbuf, 1, sizeof(inbuf),
					infile);
			if (xz_verbose) {
				fcur = ftell(infile);
				printf("compressing archive use Xz ... ... ");
				printf("\e[1;33m[%.2f%%\e[0m]\r", fcur / (float)fsize * 100);
				fflush(stdout);
			}

			if (ferror(infile)) {
				fprintf(stderr, "Read error: %s\n",
						strerror(errno));
				return false;
			}

			if (feof(infile))
				action = LZMA_FINISH;
		}

		lzma_ret ret = lzma_code(strm, action);

		if (strm->avail_out == 0 || ret == LZMA_STREAM_END) {
			size_t write_size = sizeof(outbuf) - strm->avail_out;

			if (fwrite(outbuf, 1, write_size, outfile)
					!= write_size) {
				fprintf(stderr, "Write error: %s\n",
						strerror(errno));
				return false;
			}

			strm->next_out = outbuf;
			strm->avail_out = sizeof(outbuf);
		}

		if (ret != LZMA_OK) {
			if (ret == LZMA_STREAM_END)
				return true;

			const char *msg;
			switch (ret) {
			case LZMA_MEM_ERROR:
				msg = "Memory allocation failed";
				break;

			case LZMA_DATA_ERROR:
				msg = "File size limits exceeded";
				break;

			default:
				msg = "Unknown error, possibly a bug";
				break;
			}

			fprintf(stderr, "Encoder error: %s (error code %u)\n",
					msg, ret);
			return false;
		}
	}
}


int xz_compress(const char  *infile, const char *outfile)
{
	// Get the preset number from the command line.
	uint32_t preset = 6;
	FILE *fpin = NULL, *fpout = NULL;

	if (NULL == (fpin = fopen(infile, "rb"))) {
		perror("fopen.in");
		return -1;
	}
	if (NULL == (fpout = fopen(outfile, "wb"))) {
		perror("fopen.out");
		return -1;
	}

	setvbuf(fpin, NULL, _IONBF, 0);
	setvbuf(fpout, NULL, _IONBF, 0);

	lzma_stream strm = LZMA_STREAM_INIT;

	bool success = xz_init_encoder(&strm, preset);
	if (success)
		success = xz_start_encode(&strm, fpin, fpout);

	lzma_end(&strm);

	if (fclose(fpin) || fclose(fpout)) {
		perror("fclose");
		return -1;
	}

	return success;
}

int xz_decompress(const char *infile, const char *outfile)
{
	bool ret = true;
	FILE *fpin = NULL;
	FILE *fpout = NULL;

	lzma_stream strm = LZMA_STREAM_INIT;

	if (!xz_init_decoder(&strm))
		return -1;

	if (NULL == (fpin = fopen(infile, "rb")))
		return -1;

	if (NULL == (fpout = fopen(outfile, "wb")))
		return -1;
	
	setvbuf(fpin, NULL, _IONBF, 0);
	setvbuf(fpout, NULL, _IONBF, 0);

	ret = xz_start_decode(&strm, infile, fpin, fpout);

	lzma_end(&strm);
	fclose(fpin);
	fclose(fpout);

	return ret ? 0 : -1;
}

static int archive_smear(const char *file, unsigned char magic)
{
	int fd;
	unsigned char buff[BUFSIZ];
	int nbyte;
	int i;

	if (-1 == (fd = open(file, O_RDWR))) {
		perror("open");
		return -1;
	}

	while (0 < (nbyte = read(fd, buff, BUFSIZ))) {
		for (i = 0; i < nbyte; i++)
			buff[i] ^= magic;
		lseek(fd, -nbyte, SEEK_CUR);
		if (-1 == write(fd, buff, nbyte)) {
			perror("write");
			close(fd);
			return -1;
		}
	}
	close(fd);
	return 0;
}

static inline int archive_desmear(const char *file, unsigned char  magic)
{
	archive_smear(file, magic);
	return 0;
}


int archive_pack(const char *infile, const char *outfile)
{
	char tpm[256];
	
	tar_verbose = xz_verbose = archive_verbose ? 1 : 0;

	if (NULL == tmpnam(tpm)) {
		perror("tmpname");
		return -1;
	}
	
	if (tar_verbose)
		printf("creating archive use Tar ... ... \r");
	tar_compress(infile, tpm);
	if (tar_verbose)
		printf("creating archive use Tar ... ... [\e[1;32mOK\e[0m]              \n");

	if (xz_verbose)
		printf("compressing archive use Xz ... ... \r");
	xz_compress(tpm, outfile);
	if (xz_verbose)
		printf("compressing archive use Xz ... ... [\e[1;32mOK\e[0m]            \n");
	remove(tpm);

	archive_smear(outfile, ARCHIVE_SMEAR_MAGIC);
	
	return 0;
}

int archive_unpack(const char *infile, const char *outdir)
{
	char tmp_raw[256];
	char tmp_proc[256];

	tar_verbose = xz_verbose = archive_verbose ? 1 : 0;

	if (NULL == tmpnam(tmp_raw)) {
		perror("tmpname");
		return -1;
	}
	if (NULL == tmpnam(tmp_proc)) {
		perror("tmpname");
		return -1;
	}

	if (-1 == utils_copy_file(infile, tmp_raw))
		return -1;
	
	archive_desmear(tmp_raw, ARCHIVE_SMEAR_MAGIC);

	if (xz_verbose)
		printf("decompressing archive use Xz ... ... \r");
	if (-1 == xz_decompress(tmp_raw, tmp_proc))
		return -1;
	if (xz_verbose)
		printf("decompressing archive use Xz ... ... [\e[1;32mOK\e[0m]            \n");

	if (tar_verbose)
		printf("extracting archive use Tar ... ... \r");
	if (-1 == tar_decompress(tmp_proc, outdir))
		return -1;
	if (tar_verbose)
		printf("extracting archive use Tar ... ... [\e[1;32mOK\e[0m]              \n");
	
	remove(tmp_raw);
	remove(tmp_proc);

	return 0;
}
