/*
 * Copyright 2016, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Blender Institute
 *
 */

/** \file blender/draw/modes/particle_mode.c
 *  \ingroup draw
 */

#include "DRW_engine.h"
#include "DRW_render.h"

/* If builtin shaders are needed */
#include "GPU_shader.h"

#include "draw_common.h"

#include "draw_mode_engines.h"

/* *********** LISTS *********** */
/* All lists are per viewport specific datas.
 * They are all free when viewport changes engines
 * or is free itself. Use PARTICLE_engine_init() to
 * initialize most of them and PARTICLE_cache_init()
 * for PARTICLE_PassList */

/* keep it under MAX_PASSES */
typedef struct PARTICLE_PassList {
	/* Declare all passes here and init them in
	 * PARTICLE_cache_init().
	 * Only contains (DRWPass *) */
	struct DRWPass *pass;
} PARTICLE_PassList;

/* keep it under MAX_BUFFERS */
typedef struct PARTICLE_FramebufferList {
	/* Contains all framebuffer objects needed by this engine.
	 * Only contains (GPUFrameBuffer *) */
	// struct GPUFrameBuffer *fb;
} PARTICLE_FramebufferList;

/* keep it under MAX_TEXTURES */
typedef struct PARTICLE_TextureList {
	/* Contains all framebuffer textures / utility textures
	 * needed by this engine. Only viewport specific textures
	 * (not per object). Only contains (GPUTexture *) */
	// struct GPUTexture *texture;
} PARTICLE_TextureList;

/* keep it under MAX_STORAGE */
typedef struct PARTICLE_StorageList {
	/* Contains any other memory block that the engine needs.
	 * Only directly MEM_(m/c)allocN'ed blocks because they are
	 * free with MEM_freeN() when viewport is freed.
	 * (not per object) */
	// struct CustomStruct *block;
} PARTICLE_StorageList;

typedef struct PARTICLE_Data {
	/* Struct returned by DRW_viewport_engine_data_get.
	 * If you don't use one of these, just make it a (void *) */
	// void *fbl;
	char engine_name[32]; /* Required */
	PARTICLE_FramebufferList *fbl;
	PARTICLE_TextureList *txl;
	PARTICLE_PassList *psl;
	PARTICLE_StorageList *stl;
} PARTICLE_Data;

/* *********** STATIC *********** */

/* This keeps the references of the shading groups for
 * easy access in PARTICLE_cache_populate() */
static DRWShadingGroup *group;

/* If needed, contains all global/Theme colors
 * Add needed theme colors / values to DRW_globals_update() and update UBO
 * Not needed for constant color. */
extern struct GPUUniformBuffer *globals_ubo; /* draw_common.c */
extern struct GlobalsUboStorage ts; /* draw_common.c */

/* Custom shaders :
 * Add sources to source/blender/draw/modes/shaders
 * init in PARTICLE_engine_init();
 * free in PARTICLE_engine_free(); */
static struct GPUShader *custom_shader = NULL;

/* This keeps the reference of the viewport engine data because
 * DRW_viewport_engine_data_get is slow and we don't want to
 * call it for every object */
static PARTICLE_Data *vedata;

/* *********** FUNCTIONS *********** */

/* Init Textures, Framebuffers, Storage and Shaders.
 * It is called for every frames.
 * (Optionnal) */
static void PARTICLE_engine_init(void)
{
	PARTICLE_Data *vedata = DRW_viewport_engine_data_get("ParticleMode");
	PARTICLE_TextureList *txl = vedata->txl;
	PARTICLE_FramebufferList *fbl = vedata->fbl;
	PARTICLE_StorageList *stl = vedata->stl;

	/* Init Framebuffers like this: order is attachment order (for color texs) */
	/*
	 * DRWFboTexture tex[2] = {{&txl->depth, DRW_BUF_DEPTH_24},
	 *                         {&txl->color, DRW_BUF_RGBA_8}};
	 */

	/* DRW_framebuffer_init takes care of checking if
	 * the framebuffer is valid and has the right size*/
	/*
	 * float *viewport_size = DRW_viewport_size_get();
	 * DRW_framebuffer_init(&fbl->occlude_wire_fb,
	 *                     (int)viewport_size[0], (int)viewport_size[1],
	 *                     tex, 2);
	 */

	if (!custom_shader) {
		custom_shader = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);
	}
}

/* Here init all passes and shading groups
 * Assume that all Passes are NULL */
static void PARTICLE_cache_init(void)
{
	vedata = DRW_viewport_engine_data_get("ParticleMode");
	PARTICLE_PassList *psl = vedata->psl;
	PARTICLE_StorageList *stl = vedata->stl;

	{
		/* Create a pass */
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_BLEND | DRW_STATE_WIRE;
		psl->pass = DRW_pass_create("My Pass", state);

		/* Create a shadingGroup using a function in draw_common.c or custom one */
		/*
		 * group = shgroup_dynlines_uniform_color(psl->pass, ts.colorWire);
		 * -- or --
		 * group = DRW_shgroup_create(custom_shader, psl->pass);
		 */
		group = DRW_shgroup_create(custom_shader, psl->pass);

		/* Uniforms need a pointer to it's value so be sure it's accessible at
		 * any given time (i.e. use static vars) */
		static float color[4] = {0.2f, 0.5f, 0.3f, 1.0};
		DRW_shgroup_uniform_vec4(group, "color", color, 1);
	}

}

/* Add geometry to shadingGroups. Execute for each objects */
static void PARTICLE_cache_populate(Object *ob)
{
	PARTICLE_PassList *psl = vedata->psl;
	PARTICLE_StorageList *stl = vedata->stl;

	if (ob->type == OB_MESH) {
		/* Get geometry cache */
		struct Batch *geom = DRW_cache_surface_get(ob);

		/* Add geom to a shading group */
		DRW_shgroup_call_add(group, geom, ob->obmat);
	}
}

/* Optionnal : Post-cache_populate callback */
static void PARTICLE_cache_finish(void)
{
	PARTICLE_PassList *psl = vedata->psl;
	PARTICLE_StorageList *stl = vedata->stl;

	/* Do something here! dependant on the objects gathered */
}

/* Draw time ! Control rendering pipeline from here */
static void PARTICLE_draw_scene(void)
{
	PARTICLE_Data *ved = DRW_viewport_engine_data_get("ParticleMode");
	PARTICLE_PassList *psl = ved->psl;
	PARTICLE_FramebufferList *fbl = vedata->fbl;

	/* Default framebuffer and texture */
	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

	/* Show / hide entire passes, swap framebuffers ... whatever you fancy */
	/*
	 * DRW_framebuffer_texture_detach(dtxl->depth);
	 * DRW_framebuffer_bind(fbl->custom_fb);
	 * DRW_draw_pass(psl->pass);
	 * DRW_framebuffer_texture_attach(dfbl->default_fb, dtxl->depth, 0);
	 * DRW_framebuffer_bind(dfbl->default_fb);
	 */

	/* ... or just render passes on default framebuffer. */
	DRW_draw_pass(psl->pass);

	/* If you changed framebuffer, double check you rebind
	 * the default one with its textures attached before finishing */
}

/* Cleanup when destroying the engine.
 * This is not per viewport ! only when quitting blender.
 * Mostly used for freeing shaders */
static void PARTICLE_engine_free(void)
{
	// if (custom_shader)
	// 	DRW_shader_free(custom_shader);
}

/* Create collection settings here.
 *
 * Be sure to add this function there :
 * source/blender/draw/DRW_engine.h
 * source/blender/blenkernel/intern/layer.c
 * source/blenderplayer/bad_level_call_stubs/stubs.c
 *
 * And relevant collection settings to :
 * source/blender/makesrna/intern/rna_scene.c
 * source/blender/blenkernel/intern/layer.c
 */
void PARTICLE_collection_settings_create(CollectionEngineSettings *ces)
{
	BLI_assert(ces);
	// BKE_collection_engine_property_add_int(ces, "my_bool_prop", false);
	// BKE_collection_engine_property_add_int(ces, "my_int_prop", 0);
	// BKE_collection_engine_property_add_float(ces, "my_float_prop", 0.0f);
}

DrawEngineType draw_engine_particle_type = {
	NULL, NULL,
	N_("ParticleMode"),
	&PARTICLE_engine_init,
	&PARTICLE_engine_free,
	&PARTICLE_cache_init,
	&PARTICLE_cache_populate,
	&PARTICLE_cache_finish,
	NULL, /* draw_background but not needed by mode engines */
	&PARTICLE_draw_scene
};