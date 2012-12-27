/* keecak.c */
/*
    This file is part of the AVR-Crypto-Lib.
    Copyright (C) 2010 Daniel Otte (daniel.otte@rub.de)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <avr/pgmspace.h>
#include "memxor.h"
#include "rotate64.h"
#include "keccak.h"
#include "stdio.h"

#ifdef DEBUG
#  undef DEBUG
#endif

#define DEBUG 0

#if DEBUG
#include "cli.h"

void keccak_dump_state(uint64_t a[5][5]){
	uint8_t i,j;
	for(i=0; i<5; ++i){
		cli_putstr_P(PSTR("\r\n"));
		cli_putc('0'+i);
		cli_putstr_P(PSTR(": "));
		for(j=0; j<5; ++j){
			cli_hexdump_rev(&(a[i][j]), 8);
			cli_putc(' ');
		}
	}
}

void keccak_dump_ctx(keccak_ctx_t* ctx){
	keccak_dump_state(ctx->a);
	cli_putstr_P(PSTR("\r\nDBG: r: "));
	cli_hexdump_rev(&(ctx->r), 2);
	cli_putstr_P(PSTR("\t c: "));
	cli_hexdump_rev(&(ctx->c), 2);
	cli_putstr_P(PSTR("\t d: "));
	cli_hexdump(&(ctx->d), 1);
	cli_putstr_P(PSTR("\t bs: "));
	cli_hexdump(&(ctx->bs), 1);
}

#endif


const uint8_t keccak_rc_comp[] PROGMEM = {
		0x01, 0x92, 0xda, 0x70,
		0x9b, 0x21, 0xf1, 0x59,
		0x8a, 0x88, 0x39, 0x2a,
		0xbb, 0xcb, 0xd9, 0x53,
		0x52, 0xc0, 0x1a, 0x6a,
		0xf1, 0xd0, 0x21, 0x78,
};

const uint8_t keccak_rotate_codes[5][5] PROGMEM = {
        { ROT_CODE( 0), ROT_CODE( 1), ROT_CODE(62), ROT_CODE(28), ROT_CODE(27) },
        { ROT_CODE(36), ROT_CODE(44), ROT_CODE( 6), ROT_CODE(55), ROT_CODE(20) },
        { ROT_CODE( 3), ROT_CODE(10), ROT_CODE(43), ROT_CODE(25), ROT_CODE(39) },
        { ROT_CODE(41), ROT_CODE(45), ROT_CODE(15), ROT_CODE(21), ROT_CODE( 8) },
        { ROT_CODE(18), ROT_CODE( 2), ROT_CODE(61), ROT_CODE(56), ROT_CODE(14) }
};

void keccak_f1600(uint64_t a[5][5]);

void keccak_nextBlock(keccak_ctx_t* ctx, const void* block){
	memxor(ctx->a, block, ctx->bs);
	keccak_f1600(ctx->a);
}

void keccak_lastBlock(keccak_ctx_t* ctx, const void* block, uint16_t length_b){
	while(length_b >= ctx->r){
		keccak_nextBlock(ctx, block);
		block = (uint8_t*)block + ctx->bs;
		length_b -=  ctx->r;
	}
	memxor(ctx->a, block, (length_b)/8);
	/* appand 1 */
	if(length_b & 7){
		/* we have some single bits */
		uint8_t t;
		t = ((uint8_t*)block)[length_b / 8] >> (8 - (length_b & 7));
		t |= 0x01 << (length_b & 7);
		((uint8_t*)ctx->a)[length_b / 8] ^= t;
	}else{
	    ((uint8_t*)ctx->a)[length_b / 8] ^= 0x01;
	}
	if(length_b / 8 + 1 + 3 <= ctx->bs){
        *((uint8_t*)ctx->a + length_b / 8 + 1) ^= ctx->d;
        *((uint8_t*)ctx->a + length_b / 8 + 2) ^= ctx->bs;
        *((uint8_t*)ctx->a + length_b / 8 + 3) ^= 1;
	}else{
		if(length_b / 8 + 1 + 2 <= ctx->bs){
            *((uint8_t*)ctx->a + length_b / 8 + 1) ^= ctx->d;
            *((uint8_t*)ctx->a + length_b / 8 + 2) ^= ctx->bs;
			keccak_f1600(ctx->a);
			((uint8_t*)ctx->a)[0] ^= 0x01;
		}else{
			if(length_b/8+1+1 <= ctx->bs){
				*((uint8_t*)ctx->a + length_b / 8 + 1) ^= ctx->d;
				keccak_f1600(ctx->a);
				((uint8_t*)ctx->a)[0] ^= ctx->bs;
				((uint8_t*)ctx->a)[1] ^= 0x01;
			}else{
				keccak_f1600(ctx->a);
				((uint8_t*)ctx->a)[0] ^= ctx->d;
				((uint8_t*)ctx->a)[1] ^= ctx->bs;
				((uint8_t*)ctx->a)[2] ^= 0x01;
			}
		}
	}
	keccak_f1600(ctx->a);
}

void keccak_ctx2hash(void* dest, uint16_t length_b, keccak_ctx_t* ctx){
	while(length_b>=ctx->r){
		memcpy(dest, ctx->a, ctx->bs);
		dest = (uint8_t*)dest + ctx->bs;
		length_b -= ctx->r;
		keccak_f1600(ctx->a);
	}
	memcpy(dest, ctx->a, (length_b+7)/8);
}

void keccak224_ctx2hash(void* dest, keccak_ctx_t* ctx){
	keccak_ctx2hash(dest, 224, ctx);
}

void keccak256_ctx2hash(void* dest, keccak_ctx_t* ctx){
	keccak_ctx2hash(dest, 256, ctx);
}

void keccak384_ctx2hash(void* dest, keccak_ctx_t* ctx){
	keccak_ctx2hash(dest, 384, ctx);
}

void keccak512_ctx2hash(void* dest, keccak_ctx_t* ctx){
	keccak_ctx2hash(dest, 512, ctx);
}

/*
  1. SHA3-224: ⌊Keccak[r = 1152, c =  448, d = 28]⌋224
  2. SHA3-256: ⌊Keccak[r = 1088, c =  512, d = 32]⌋256
  3. SHA3-384: ⌊Keccak[r =  832, c =  768, d = 48]⌋384
  4. SHA3-512: ⌊Keccak[r =  576, c = 1024, d = 64]⌋512
*/
void keccak_init(uint16_t r, uint16_t c, uint8_t d, keccak_ctx_t* ctx){
	memset(ctx->a, 0x00, 5 * 5 * 8);
	ctx->r = r;
	ctx->c = c;
	ctx->d = d;
	ctx->bs = (uint8_t)(r / 8);
}

void keccak224_init(keccak_ctx_t* ctx){
	keccak_init(1152, 448, 28, ctx);
}

void keccak256_init(keccak_ctx_t* ctx){
	keccak_init(1088, 512, 32, ctx);
}

void keccak384_init(keccak_ctx_t* ctx){
	keccak_init( 832, 768, 48, ctx);
}

void keccak512_init(keccak_ctx_t* ctx){
	keccak_init( 576, 1024, 64, ctx);
}
