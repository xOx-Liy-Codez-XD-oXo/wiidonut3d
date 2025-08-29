#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <aesndlib.h>
#include <gcmodplay.h>

#include "graphicsUtililiys.h"
#include "primitives.h"

#include "Torus_liym3q.h"

#include "music_mod.h"

#define DEFAULT_FIFO_SIZE	(256*1024)

static GXRModeObj *rmode = NULL;
static void *frameBuffer[2] = { NULL, NULL};
static MODPlay play;

int main(int argc,char **argv) {

	f32 yscale = 0;
	u32 xfbHeight;
	u32 fb = 0;
	u64 frame = 0;
	Mtx view; // view and perspective matrices
	Mtx model, model2, modelview;
	Mtx44 perspective;
	void *gpfifo = NULL;
	GXColor background = {0, 0, 0, 0xff};
	guVector cam = {0.0F, 0.0F, 0.0F},
			up = {0.0F, 1.0F, 0.0F},
		  look = {0.0F, 0.0F, -1.0F};

	VIDEO_Init();
	WPAD_Init();
	AESND_Init();

	rmode = VIDEO_GetPreferredMode(NULL);

	rmode->viWidth = 710;
	rmode->viXOrigin = 5;

	// allocate the fifo buffer
	gpfifo = memalign(32,DEFAULT_FIFO_SIZE);
	memset(gpfifo,0,DEFAULT_FIFO_SIZE);

	// allocate 2 framebuffers for double buffering
	frameBuffer[0] = SYS_AllocateFramebuffer(rmode);
	frameBuffer[1] = SYS_AllocateFramebuffer(rmode);

	// configure video
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(frameBuffer[fb]);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	fb ^= 1;

	// init the flipper
	GX_Init(gpfifo,DEFAULT_FIFO_SIZE);

	// clears the bg to color and clears the z buffer
	GX_SetCopyClear(background, 0x00ffffff);

	// other gx setup
	GX_SetViewport(0,0,rmode->fbWidth,rmode->efbHeight,0,1);
	yscale = GX_GetYScaleFactor(rmode->efbHeight,rmode->xfbHeight);
	xfbHeight = GX_SetDispCopyYScale(yscale);
	GX_SetScissor(0,0,rmode->fbWidth,rmode->efbHeight);
	GX_SetDispCopySrc(0,0,rmode->fbWidth,rmode->efbHeight);
	GX_SetDispCopyDst(rmode->fbWidth,xfbHeight);
	GX_SetCopyFilter(rmode->aa,rmode->sample_pattern,GX_TRUE,rmode->vfilter);
	GX_SetFieldMode(rmode->field_rendering,((rmode->viHeight==2*rmode->xfbHeight)?GX_ENABLE:GX_DISABLE));

	if (rmode->aa) {
		GX_SetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
	} else {
		GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
	}

	GX_SetCullMode(GX_CULL_NONE);
	GX_CopyDisp(frameBuffer[fb],GX_TRUE);
	GX_SetDispCopyGamma(GX_GM_1_0);

	ag_config_Torus(0);

	// setup our camera at the origin
	// looking down the -z axis with y up
	guLookAt(view, &cam, &up, &look);

	// setup our projection matrix
	float aspect;
	if (CONF_GetAspectRatio() == CONF_ASPECT_16_9) {
		aspect = 16.0f/9.0f;
	} else {
		aspect = 4.0f/3.0f;
	}

	guPerspective(perspective, 45, aspect, 0.1F, 300.0F);
	GX_LoadProjectionMtx(perspective, GX_PERSPECTIVE);

	//set number of textures to generate
	GX_SetNumTexGens(1);

	// setup texture coordinate generation
	// args: texcoord slot 0-7, matrix type, source to generate texture coordinates from, matrix to use
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);

	guVector lpos = {2.0f, 2.0f, 2.0f};
	GXLightObj lobj;	

	GX_SetNumChans(1);

	static GXColor white = {255, 255, 255, 255};
	static GXColor darkgrey = {16, 16, 16, 255};

	guVecMultiply(view, &lpos, &lpos);

	GX_InitLightPos(&lobj,lpos.x,lpos.y,lpos.z);
	GX_InitLightColor(&lobj, white);
   	GX_InitLightDistAttn(&lobj, 0.5f, 0.5f, GX_DA_MEDIUM);
	GX_LoadLightObj(&lobj,GX_LIGHT0);

	GX_SetChanAmbColor(GX_COLOR0A0, darkgrey);
	GX_SetChanMatColor(GX_COLOR0A0, white);
	GX_SetChanCtrl(GX_COLOR0A0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0, GX_DF_CLAMP, GX_AF_NONE);

	MODPlay_Init(&play);
	MODPlay_SetMOD(&play, music_mod);
	MODPlay_SetVolume(&play,63,63);
	MODPlay_Start(&play);

	while(1) {

		WPAD_ScanPads();
		if(WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) exit(0);

		guMtxRotRad(model, 'x', (float)frame * 0.01f);
		guMtxRotRad(model2, 'y', (float)frame * 0.02f);
		guMtxConcat(model, model2, model);
		guMtxTransApply(model, model, 0.0f, 0.0f, -4.0f);
		guMtxConcat(view,model,modelview);
		// load the modelview matrix into matrix memory
		GX_LoadPosMtxImm(modelview, GX_PNMTX0);
		GX_LoadNrmMtxImm(modelview, GX_PNMTX0);
		GX_SetCurrentMtx(GX_PNMTX0);

		ag_draw_Torus(0);

		GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
		GX_SetColorUpdate(GX_TRUE);
		GX_CopyDisp(frameBuffer[fb],GX_TRUE);

		GX_DrawDone();

		VIDEO_SetNextFramebuffer(frameBuffer[fb]);
		if(!frame) {
			VIDEO_SetBlack(false);
		}
		VIDEO_Flush();
		VIDEO_WaitVSync();
		fb ^= 1;

		frame++;
	}
}
