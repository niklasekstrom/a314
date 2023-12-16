/*
 * Copyright (c) 2018 Niklas Ekström
 */

#include <Python.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define COLORS 8
#define CHUNKLEN 255

struct _Node
{
	uint16_t children[COLORS];
};
typedef struct _Node Node;

static Node *nodes;

static void init_node(uint16_t code)
{
	Node *node = &nodes[code];
	for (int i = 0; i < COLORS; i++)
		node->children[i] = 0xffff;
}

static uint8_t *buffer;
static int capacity;
static int length;

static void append_byte(uint8_t b)
{
	if (length == capacity)
	{
		capacity *= 2;
		buffer = realloc(buffer, capacity);
	}

	buffer[length++] = b;
}

static int chunk_left = 0;

static void flush_byte(uint8_t b)
{
	if (!chunk_left)
	{
		append_byte(CHUNKLEN);
		chunk_left = CHUNKLEN;
	}

	append_byte(b);
	chunk_left--;
}

#define c2p(c) &nodes[c]
#define p2c(p) (((uint32_t)((intptr_t)p - (intptr_t)nodes)) / sizeof(Node))

static const uint32_t clear_code = 256;
static const uint32_t eoi_code = 257;
static uint32_t next_code = 258;

static int clen;

static uint32_t buf;
static int blen;

static Node *prefix;

static void begin_encode(void)
{
	chunk_left = 0;

	for (int i = 0; i < COLORS; i++)
		init_node(i);

	next_code = 258;
	clen = 9;

	buf = 0;
	blen = 0;

	buf |= clear_code << blen;
	blen += clen;

	prefix = NULL;
}

static void end_encode(void)
{
	buf |= p2c(prefix) << blen;
	blen += clen;

	buf |= eoi_code << blen;
	blen += clen;

	while (blen > 0)
	{
		flush_byte(buf);
		buf >>= 8;
		blen -= 8;
	}

	if (chunk_left)
	{
		int used = CHUNKLEN - chunk_left;
		buffer[length - used - 1] = used;
	}
}

static void encode(uint8_t *pixels, int count)
{
	uint32_t *cp = (uint32_t *)pixels;
	uint32_t *end = (uint32_t *)(pixels + (count / 2));

	uint32_t ibuf = *cp++;
	int ilen = 8;

	if (!prefix)
	{
		uint8_t K = ibuf & 0xf;
		ibuf >>= 4;
		ilen--;
		prefix = &nodes[K];
	}

	while (cp < end || ilen)
	{
		if (!ilen)
		{
			ibuf = *cp++;
			ilen = 8;
		}

		uint8_t K = ibuf & 0xf;
		ibuf >>= 4;
		ilen--;

		if (prefix->children[K] != 0xffff)
			prefix = c2p(prefix->children[K]);
		else
		{
			buf |= p2c(prefix) << blen;
			blen += clen;

			init_node(next_code);
			prefix->children[K] = next_code;

			if (next_code == (1U << clen))
				clen++;
			next_code++;

			prefix = &nodes[K];

			if (next_code == 4095)
			{
				buf |= clear_code << blen;
				blen += clen;

				for (int i = 0; i < COLORS; i++)
					init_node(i);

				clen = 9;
				next_code = 258;
			}

			while (blen >= 8)
			{
				flush_byte(buf);
				buf >>= 8;
				blen -= 8;
			}
		}
	}
}

#define W 640
#define H 256
#define BLOCK_HEIGHT 32
#define BPL_SIZE (W * H / 8)
#define BPL_COUNT 3

static uint8_t pal[8*3] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static void copy_bpls_to_pixels(uint8_t *pixels, uint8_t *bpls, int b)
{
	memset(pixels, 0, W * BLOCK_HEIGHT / 2);

	bpls += b * BLOCK_HEIGHT * W / 8;
	for (int i = 0; i < BPL_COUNT; i++)
	{
		uint8_t *src = bpls;
		uint32_t *dst = (uint32_t *)pixels;

		int shift = 28 - (7 - i);

		for (int j = 0; j < BLOCK_HEIGHT * W / 8; j++)
		{
			uint8_t x = *src++;
			uint32_t bits = 0;
			for (int k = 0; k < 8; k++)
			{
				bits = (bits >> 4) | ((x & 0x80) << shift);
				x <<= 1;
			}
			*dst++ |= bits;
		}
		bpls += BPL_SIZE;
	}
}

static uint8_t *pixels;

static void write_gif(uint8_t *bpls)
{
	uint8_t *p = buffer;
	memcpy(p, "GIF89a", 6);
	p += 6;
	*((short *)p) = W;
	p += 2;
	*((short *)p) = H;
	p += 2;
	*p++ = 0xf7;
	*p++ = 0;
	*p++ = 0;
	memset(p, 0, 768);
	memcpy(p, pal, sizeof(pal));
	p += 768;
	*p++ = 0x2c;
	*((short *)p) = 0;
	p += 2;
	*((short *)p) = 0;
	p += 2;
	*((short *)p) = W;
	p += 2;
	*((short *)p) = H;
	p += 2;
	*p++ = 0;
	*p++ = 8;
	length = p - buffer;

	begin_encode();
	for (int b = 0; b < H / BLOCK_HEIGHT; b++)
	{
		copy_bpls_to_pixels(pixels, bpls, b);
		encode(pixels, W * BLOCK_HEIGHT);
	}
	end_encode();

	append_byte(0);
	append_byte(0x3b);
}

#if PY_MAJOR_VERSION > 2
#define BYTEARRAY_FORMAT "y#"
#else
#define BYTEARRAY_FORMAT "s#"
#endif

static PyObject *b2g_set_palette(PyObject *self, PyObject *args)
{
	char *buf;
	int len;

	if (!PyArg_ParseTuple(args, BYTEARRAY_FORMAT, &buf, &len))
		return NULL;

	if (len != 24) {
		PyErr_SetString(PyExc_RuntimeError, "Must be 8*3 = 24 bytes (r, g, b).");
		return NULL;
	}

	memcpy(pal, buf, 24);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *b2g_encode(PyObject *self, PyObject *args)
{
	char *buf;
	int len;

	if (!PyArg_ParseTuple(args, BYTEARRAY_FORMAT, &buf, &len))
		return NULL;

	if (len != 80*256*3) {
		PyErr_SetString(PyExc_RuntimeError, "Must be 3 bitplanes of 256 rows with 80 bytes per row (total of 61440 bytes).");
		return NULL;
	}

	write_gif((uint8_t *)buf);

	return Py_BuildValue(BYTEARRAY_FORMAT, buffer, length);
}

static char module_docstring[] =
"Encode an image given as a set of Amiga bitplanes as a GIF file.";

static char set_palette_docstring[] =
"Set palette to use for image. Input is a string of r, g, b color values.";

static char encode_docstring[] =
"Encode bitplanes as GIF, using the palette given previously.";

static PyMethodDef module_methods[] = {
	{"set_palette", b2g_set_palette, METH_VARARGS, set_palette_docstring },
	{"encode", b2g_encode, METH_VARARGS, encode_docstring},
	{NULL, NULL, 0, NULL}
};

#if PY_MAJOR_VERSION > 2
static struct PyModuleDef bpls2gif_module = {
   PyModuleDef_HEAD_INIT,
   "bpls2gif",
   module_docstring,
   -1,
   module_methods
};
#endif

#if PY_MAJOR_VERSION > 2
PyMODINIT_FUNC PyInit_bpls2gif(void)
#else
PyMODINIT_FUNC initbpls2gif(void)
#endif
{
#if PY_MAJOR_VERSION > 2
	PyObject *m = PyModule_Create(&bpls2gif_module);
	if (m == NULL)
		return NULL;
#else
	PyObject *m = Py_InitModule3("bpls2gif", module_methods, module_docstring);
	if (m == NULL)
		return;
#endif

	nodes = malloc(4096 * sizeof(Node));
	pixels = malloc(W * BLOCK_HEIGHT / 2);

	buffer = malloc(4096);
	capacity = 4096;
	length = 0;

#if PY_MAJOR_VERSION > 2
	return m;
#else
	return;
#endif
}
