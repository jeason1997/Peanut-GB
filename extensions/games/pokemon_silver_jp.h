/* Presentation adapter for unmodified Japanese Pokemon Silver revision 1.
 * SHA1 a11d5ddc26eb826086593f82370b15d16404d33e, CRC32 0aea5383.
 * Memory layout and metatile algorithm verified against that ROM and
 * https://github.com/Narishma-gb/pokesilver/tree/edbe53978ef1777fc5c41019e17b7544070eab92
 * (home/map.asm, engine/overworld/player_step.asm, load_map_part.asm).
 * No game assets or ROM-file writes. Rendering never writes emulated RAM.
 * Optional ROM-read policy expands NPC life/AI and restores remembered facing;
 * instruction timings and hardware PPU timing remain unchanged.
 * Include after peanut_gb.h. All buffers/state are owned by the frontend.
 */
#ifndef PEANUT_POKEMON_SILVER_JP_H
#define PEANUT_POKEMON_SILVER_JP_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "../include/gx_gb_view.h"
#ifndef PGB_SILVER_STATE
#define PGB_SILVER_STATE gx_gb_view
#endif
#ifndef PGB_SILVER_LAYER_TAGS
#define PGB_SILVER_LAYER_TAGS 1
#endif
#ifndef PGB_SILVER_HIGH_ACCURACY
#define PGB_SILVER_HIGH_ACCURACY 1
#endif
#ifndef PGB_SILVER_MAX_MAPS
#define PGB_SILVER_MAX_MAPS 64
#endif
#if PGB_SILVER_MAX_MAPS < 1 || PGB_SILVER_MAX_MAPS > 64
#error PGB_SILVER_MAX_MAPS must be in 1..64
#endif
#ifndef PGB_SILVER_BLOCK_CACHE
#define PGB_SILVER_BLOCK_CACHE 16
#endif
#if PGB_SILVER_BLOCK_CACHE != 0 && PGB_SILVER_BLOCK_CACHE != 16
#error PGB_SILVER_BLOCK_CACHE must be 0 or 16
#endif
/* Runtime switches affect work; compile-time capacities affect reserved RAM. */
enum pgb_silver_feature {
 PGB_SILVER_TERRAIN = 1u << 0,
 PGB_SILVER_SPRITES = 1u << 1,
 PGB_SILVER_NPC_LIFETIME = 1u << 2,
 PGB_SILVER_FACING_MEMORY = 1u << 3,
 PGB_SILVER_CONNECTIONS = 1u << 4,
 PGB_SILVER_STABLE_BORDER = 1u << 5,
 PGB_SILVER_NEIGHBOR_NPCS = 1u << 6,
 PGB_SILVER_ALL = (1u << 7) - 1
};
#ifndef PGB_SILVER_DEFAULT_FEATURES
#define PGB_SILVER_DEFAULT_FEATURES PGB_SILVER_ALL
#endif
#ifndef PGB_SILVER_MAX_OBJECTS
#define PGB_SILVER_MAX_OBJECTS 256
#endif
#if PGB_SILVER_MAX_OBJECTS < 28
#error PGB_SILVER_MAX_OBJECTS must retain the 28 current-map slots
#endif

struct pgb_silver_object
{
	int x, y;
	uint32_t graphics; /* 0 = VRAM; otherwise ROM offset of standing graphics */
	uint16_t order;
	uint8_t facing, tile, flags, grass, tiles;
	uint8_t map_index, live, oam_pose;
	uint8_t map_group, map_number, preview;
};

struct pgb_silver_map
{
	int width, height, origin_x, origin_y;
	uint32_t definitions;
	uint8_t border;
};

struct pgb_silver_connection
{
	int x, y, width, height;
	uint32_t blocks, attributes;
	uint8_t group, number, border;
};

struct pgb_silver_npc_memory
{
	uint32_t identity;
	int world_x, world_y;
	uint8_t valid, direction, facing, record_x, record_y;
};

struct pgb_silver_jp
{
	const uint8_t *rom;
	size_t rom_size;
	int supported;
	int sprites_enabled; /* legacy master switch for F9 */
	unsigned int features;
	unsigned int object_limit;
	unsigned int objects_dropped;
	int atlas_truncated;
	int sprite_ready;
	unsigned int object_count;
	struct pgb_silver_object objects[PGB_SILVER_MAX_OBJECTS];
	uint16_t viewport_width, viewport_height;
	/* Presentation snapshot: a row must never change scene validity mid-frame. */
	int frame_ready, frame_valid;
	uint8_t frame_scx, frame_scy;
	unsigned int last_native_line;
	struct pgb_silver_map frame_map;
	uint8_t frame_blocks[1300];
	struct pgb_silver_connection connections[4];
	/* Stable map-relative layout of the connected, same-tileset world. */
	struct pgb_silver_connection atlas[PGB_SILVER_MAX_MAPS];
	unsigned int atlas_count;
	uint8_t atlas_tileset;
	/* Small direct-mapped cache; frame snapshot fixes block ownership. */
	#if PGB_SILVER_BLOCK_CACHE
	struct { int x, y; uint8_t block; } block_cache[PGB_SILVER_BLOCK_CACHE];
	uint16_t block_cache_valid;
#endif
	/* Version-specific ROM-read compatibility policy; original ROM is immutable. */
	uint8_t pad_x, pad_y, history_group, history_number;
	struct pgb_silver_npc_memory npc[16];
};

static int pgb_silver_jp_init(struct pgb_silver_jp *s, const uint8_t *rom, size_t size)
{
	uint32_t crc = 0xffffffffu;
	size_t i;
	s->rom = rom;
	s->rom_size = size;
	s->supported = 0;
	s->sprites_enabled = 1;
	s->features = PGB_SILVER_DEFAULT_FEATURES;
	s->object_limit = PGB_SILVER_MAX_OBJECTS;
	s->objects_dropped = 0; s->atlas_truncated = 0;
	s->sprite_ready = 0;
	s->object_count = 0;
	s->viewport_width = 160; s->viewport_height = 144;
	s->frame_ready = s->frame_valid = 0;
	s->frame_scx = s->frame_scy = 0;
	s->last_native_line = GX_GB_HEIGHT;
	s->pad_x = s->pad_y = s->history_group = s->history_number = 0;
	memset(s->npc, 0, sizeof(s->npc));
	memset(s->connections, 0, sizeof(s->connections));
	s->atlas_count = 0; s->atlas_tileset = 0;
	if(rom == NULL || size != 1048576)
		return 0;
	for(i = 0; i < size; i++)
	{
		unsigned int bit;
		crc ^= rom[i];
		for(bit = 0; bit < 8; bit++)
			crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
	}
	s->supported = (crc ^ 0xffffffffu) == 0x0aea5383u;
	return s->supported;
}

/* Returns 0 for contradictory feature settings or out-of-range budgets.
 * Changes take effect at the next explicit/automatic frame begin. */
static int pgb_silver_jp_configure(struct pgb_silver_jp *s,
        unsigned int features, unsigned int object_limit)
{
    if((features & ~PGB_SILVER_ALL) || object_limit < 28 || object_limit > PGB_SILVER_MAX_OBJECTS)
        return 0;
    if((features & (PGB_SILVER_SPRITES | PGB_SILVER_FACING_MEMORY | PGB_SILVER_CONNECTIONS)) && !(features & PGB_SILVER_TERRAIN)) return 0;
    if((features & (PGB_SILVER_STABLE_BORDER | PGB_SILVER_NEIGHBOR_NPCS)) && !(features & PGB_SILVER_CONNECTIONS)) return 0;
    if((features & PGB_SILVER_NEIGHBOR_NPCS) && !(features & PGB_SILVER_SPRITES)) return 0;
    s->features = features; s->object_limit = object_limit;
    s->frame_ready = s->frame_valid = 0; s->atlas_count = 0;
    return 1;
}

static uint32_t pgb_silver_npc_identity(const uint8_t *record)
{
	return record[1] | ((uint32_t)record[4] << 8) |
		((uint32_t)record[10] << 16) | ((uint32_t)record[11] << 24);
}

static const struct pgb_silver_npc_memory *pgb_silver_npc_memory(
		const struct pgb_silver_jp *s, const struct PGB_SILVER_STATE *gb, unsigned int index)
{
	const struct pgb_silver_npc_memory *memory;
	if(index == 0 || index >= 16 || gb->wram[0x1538 + index] || s->history_group != gb->wram[0x19c6] ||
		s->history_number != gb->wram[0x19c7]) return NULL;
	memory = &s->npc[index];
	return memory->valid && memory->record_x == gb->wram[0x1438 + index * 16 + 3] &&
		memory->record_y == gb->wram[0x1438 + index * 16 + 2] &&
		memory->identity == pgb_silver_npc_identity(
		gb->wram + 0x1438 + index * 16) ? memory : NULL;
}

/* Keep a guard band beyond the expanded view: do not delete an NPC while
 * even part of it is still visible. These are 16-pixel logical map tiles. */
static void pgb_silver_jp_viewport(struct pgb_silver_jp *s, uint16_t width, uint16_t height)
{
	unsigned int x = width > 160 ? ((unsigned int)width - 160 + 31) / 32 : 0;
	unsigned int y = height > 144 ? ((unsigned int)height - 144 + 31) / 32 : 0;
	s->viewport_width = width; s->viewport_height = height;
	s->pad_x = x > 31 ? 31 : x;
	s->pad_y = y > 31 ? 31 : y;
}

static int pgb_silver_signed(unsigned int value)
{
	return (int)(value & 255) - ((value & 128) ? 256 : 0);
}

static int pgb_silver_map_state(const struct pgb_silver_jp *s,
		const struct PGB_SILVER_STATE *gb, struct pgb_silver_map *map);

/* Lift hardware coordinates into the object's map neighbourhood. Both the
 * OAM admission policy and presentation must use this same coordinate space. */
static void pgb_silver_live_position(const struct PGB_SILVER_STATE *gb,
		const struct pgb_silver_map *map, const uint8_t *o, int *x, int *y)
{
	int expected_x = (o[16] + 2) * 16 - map->origin_x;
	int expected_y = (o[17] + 2) * 16 - map->origin_y - 4;
	*x = expected_x + pgb_silver_signed(o[23] + o[25] + gb->wram[0xe75] - expected_x);
	*y = expected_y + pgb_silver_signed(o[24] + o[26] + gb->wram[0xe76] - 4 - expected_y);
}

/* Read-only overlay on the exact JP Rev 1 ROM. Only existing immediate
 * constants/data change; no opcodes or instruction lengths/timings change.
 * NPC AI/visibility intentionally extends with the viewport. No disk writes.
 */
static uint8_t pgb_silver_jp_rom_read(const struct pgb_silver_jp *s,
		const struct PGB_SILVER_STATE *gb, uint_fast32_t address)
{
	uint8_t value = s->rom[address];
	if(!s->supported || !(s->pad_x || s->pad_y)) return value;
	/* InitSprites.DeterminePriorities: CP STANDING at $592e. A far-away
	 * live NPC must not export its modulo-256 coordinates to hardware OAM:
	 * x=-98 otherwise becomes x=158 and draws on the opposite screen edge.
	 * Comparing the current facing takes the existing skip branch. The NPC
	 * remains alive and is still rendered at its full map position below.
	 * This changes neither opcodes nor instruction sizes nor PPU timing. */
	if((s->features & PGB_SILVER_NPC_LIFETIME) && address == 0x592f && gb->cpu_reg.pc.reg == 0x5930)
	{
		unsigned int bc = gb->cpu_reg.bc.reg;
		struct pgb_silver_map map;
		if(bc >= 0xd1f0 && bc < 0xd1f0 + 13 * 40 && (bc - 0xd1f0) % 40 == 0 &&
			pgb_silver_map_state(s, gb, &map))
		{
			const uint8_t *o = gb->wram + bc - 0xc000;
			int x, y;
			pgb_silver_live_position(gb, &map, o, &x, &y);
			if(x <= -32 || x >= 176 || y <= -32 || y >= 160) return o[13];
		}
	}
	/* CheckObjectStillVisible: live and initial positions, X then Y.
	 * InitializeVisibleSprites uses the same enlarged bounds. */
	if(s->features & PGB_SILVER_NPC_LIFETIME) switch(address)
	{
	/* Scan the whole enlarged window each player step, using the existing
	 * initializer. Shifting only the incoming edge would miss NPCs already
	 * between the old edge and the newly expanded edge. */
	case 0x9675a: return 0x67; /* farcall $02:47ba -> $02:4767 */
	case 0x4397: case 0x43c3: case 0x878d:
		return value + s->pad_x;
	case 0x439c: case 0x43c8: case 0x8792:
		return value + s->pad_x * 2;
	case 0x43a9: case 0x43d5: case 0x879b:
		return value + s->pad_y;
	case 0x43ae: case 0x43da: case 0x87a0:
		return value + s->pad_y * 2;
	default: break;
	}
	/* CopySpriteMovementData's LD A,[BC] at $18d0 reads the initial facing.
	 * Restore the last real facing for the SAME NPC when the game respawns
	 * it. The CPU still executes all original initialization instructions. */
	if((s->features & PGB_SILVER_FACING_MEMORY) && address >= 0x4275 && address < 0x4274 + 37 * 6 &&
		(address - 0x4275) % 6 == 0 && gb->cpu_reg.pc.reg == 0x18d1)
	{
		unsigned int index = gb->wram[0xe8d];
		const struct pgb_silver_npc_memory *memory = pgb_silver_npc_memory(s, gb, index);
		if(memory && gb->wram[0xe8e] == (uint8_t)memory->identity &&
			gb->wram[0xe91] == (uint8_t)(memory->identity >> 8))
			return memory->direction >> 2;
	}
	return value;
}

static uint16_t pgb_silver_word(const struct PGB_SILVER_STATE *gb, unsigned int address)
{
	return gb->wram[address - 0xc000] | (gb->wram[address - 0xc000 + 1] << 8);
}

/* Arithmetic floor division, including negative offscreen pixel coordinates. */
static int pgb_silver_floor32(int value)
{
	return value >= 0 ? value / 32 : -((-value + 31) / 32);
}

static uint16_t pgb_silver_rom_word(const struct pgb_silver_jp *s, uint32_t at)
{
	return s->rom[at] | (s->rom[at + 1] << 8);
}

/* Verified map header table: bank $25, MapGroupPointers=$40ed, entry size 9.
 * All descriptors reference the original ROM; no graphics/maps are copied. */
static int pgb_silver_region(const struct pgb_silver_jp *s, unsigned int group,
		unsigned int number, unsigned int tileset, struct pgb_silver_connection *out)
{
	unsigned int pointer, header, bank, attr, end;
	if(!group || group > 26 || !number) return 0;
	pointer = pgb_silver_rom_word(s, 0x940ed + (group - 1) * 2);
	if(pointer < 0x4000 || pointer >= 0x8000) return 0;
	header = 0x90000u + pointer + (number - 1) * 9;
	end = group == 26 ? 0x90000u + pointer + 11 * 9 :
		0x90000u + pgb_silver_rom_word(s, 0x940ed + group * 2);
	if(header + 9 > end || header + 9 > 0x98000 || s->rom[header + 1] != tileset) return 0;
	bank = s->rom[header]; pointer = pgb_silver_rom_word(s, header + 3);
	if(!bank || bank >= 64 || pointer < 0x4000 || pointer > 0x7ff4) return 0;
	attr = bank * 0x4000u + pointer - 0x4000u;
	unsigned int height = s->rom[attr + 1], width = s->rom[attr + 2];
	bank = s->rom[attr + 3]; pointer = pgb_silver_rom_word(s, attr + 4);
	if(!width || !height || !bank || bank >= 64 || pointer < 0x4000 ||
		pointer + width * height > 0x8000) return 0;
	memset(out, 0, sizeof(*out));
	out->width = width; out->height = height;
	out->blocks = bank * 0x4000u + pointer - 0x4000u;
	out->attributes = attr; out->border = s->rom[attr];
	out->group = group; out->number = number;
	return 1;
}

static void pgb_silver_place_region(const struct pgb_silver_connection *parent,
		const uint8_t *connection, unsigned int side, struct pgb_silver_connection *child)
{
	child->x = side == 2 ? parent->x - child->width : side == 3 ? parent->x + parent->width :
		parent->x - pgb_silver_signed(connection[9]) / 2;
	child->y = side == 0 ? parent->y - child->height : side == 1 ? parent->y + parent->height :
		parent->y - pgb_silver_signed(connection[8]) / 2;
}

/* Direct connections also establish continuity during MAPSTATUS_ENTER. */
static void pgb_silver_connections(struct pgb_silver_jp *s, const struct PGB_SILVER_STATE *gb,
		const struct pgb_silver_map *map)
{
	struct pgb_silver_connection parent = {0};
	parent.x = parent.y = 3; parent.width = map->width - 6; parent.height = map->height - 6;
	memset(s->connections, 0, sizeof(s->connections));
	for(unsigned int side = 0; side < 4; side++)
	{
		const uint8_t *c = gb->wram + 0x1084 + side * 12;
		struct pgb_silver_connection *out = &s->connections[side];
		if((gb->wram[0x1083] & (8u >> side)) &&
			pgb_silver_region(s, c[0], c[1], gb->wram[0x1074], out))
			pgb_silver_place_region(&parent, c, side, out);
	}
}

/* Keep one physical layout across connected map changes. Rebasing translates
 * every region equally, so ownership of ocean/forest outside the map rectangles
 * cannot change when the player crosses a connection. ROM links complete the
 * component, avoiding the changing neighbour set of a one-hop lookup. */
static void pgb_silver_atlas(struct pgb_silver_jp *s, const struct PGB_SILVER_STATE *gb,
		const struct pgb_silver_map *map)
{
	unsigned int group = gb->wram[0x19c6], number = gb->wram[0x19c7];
	if(s->atlas_tileset == gb->wram[0x1074])
		for(unsigned int i = 0; i < s->atlas_count; i++)
			if(s->atlas[i].group == group && s->atlas[i].number == number)
			{
				int dx = s->atlas[i].x - 3, dy = s->atlas[i].y - 3;
				for(unsigned int j = 0; j < s->atlas_count; j++)
				{ s->atlas[j].x -= dx; s->atlas[j].y -= dy; }
				s->atlas[i].border = map->border;
				return;
			}
	s->atlas_count = 0; s->atlas_truncated = 0; s->atlas_tileset = gb->wram[0x1074];
	if(!pgb_silver_region(s, group, number, s->atlas_tileset, &s->atlas[0])) return;
	s->atlas[0].x = s->atlas[0].y = 3; s->atlas[0].border = map->border; s->atlas_count = 1;
	for(unsigned int i = 0; i < s->atlas_count; i++)
	{
		const struct pgb_silver_connection *parent = &s->atlas[i];
		unsigned int at = parent->attributes + 12, flags = s->rom[parent->attributes + 11];
		for(unsigned int side = 0; side < 4; side++)
		{
			if(!(flags & (8u >> side))) continue;
			const uint8_t *c = s->rom + at; at += 12;
			unsigned int j;
			for(j = 0; j < s->atlas_count; j++)
				if(s->atlas[j].group == c[0] && s->atlas[j].number == c[1]) break;
			if(j < s->atlas_count) continue;
			if(j == PGB_SILVER_MAX_MAPS) { s->atlas_truncated = 1; continue; }
			if(!pgb_silver_region(s, c[0], c[1], s->atlas_tileset, &s->atlas[j])) continue;
			pgb_silver_place_region(parent, c, side, &s->atlas[j]);
			s->atlas_count++;
		}
	}
}

static uint8_t pgb_silver_block(const struct pgb_silver_jp *s,
		const struct PGB_SILVER_STATE *gb, const struct pgb_silver_map *m, int x, int y)
{
	int bx = pgb_silver_floor32(x), by = pgb_silver_floor32(y);
	uint8_t block = 0;
	if(bx >= 0 && by >= 0 && bx < m->width && by < m->height)
		block = (m == &s->frame_map ? s->frame_blocks : gb->wram + 0x700)
			[by * m->width + bx];
	/* Preserve live RAM, including scripted block changes, wherever the
	 * engine has loaded it. Only extend beyond the padded RAM rectangle. */
	else if(m == &s->frame_map)
		for(unsigned int side = 0; side < 4; side++)
		{
			const struct pgb_silver_connection *c = &s->connections[side];
			int cx = bx - c->x, cy = by - c->y;
			if(c->blocks && cx >= 0 && cy >= 0 && cx < c->width && cy < c->height)
			{
				block = s->rom[c->blocks + cy * c->width + cx];
				break;
			}
		}
	if(block == 0)
		block = m->border;
	return block;
}

static uint8_t pgb_silver_tile(const struct pgb_silver_jp *s,
		const struct PGB_SILVER_STATE *gb, const struct pgb_silver_map *m, int x, int y)
{
	uint8_t block = pgb_silver_block(s, gb, m, x, y);
	/* Match the original 8-bit doubling in LoadMetatiles (IDs wrap at 128). */
	return s->rom[m->definitions + (block & 127) * 16 +
		(((uint32_t)y & 31) / 8) * 4 + ((uint32_t)x & 31) / 8];
}

/* Extended-area terrain has spatial ownership rather than a current-map
 * default. The nearest map rectangle supplies its own border block outside
 * real terrain; a stable map ID breaks ties independently of traversal order. */
static uint8_t pgb_silver_world_block(const struct pgb_silver_jp *s,
		const struct PGB_SILVER_STATE *gb, const struct pgb_silver_map *map, int x, int y)
{
	int bx = pgb_silver_floor32(x), by = pgb_silver_floor32(y);
	unsigned int best = 0, best_key = ~0u;
	uint64_t distance = UINT64_MAX;
	if(!(s->features & PGB_SILVER_STABLE_BORDER) || !s->atlas_count) return pgb_silver_block(s, gb, map, x, y);
	for(unsigned int i = 0; i < s->atlas_count; i++)
	{
		const struct pgb_silver_connection *c = &s->atlas[i];
		int dx = bx < c->x ? c->x - bx : bx >= c->x + c->width ? bx - c->x - c->width + 1 : 0;
		int dy = by < c->y ? c->y - by : by >= c->y + c->height ? by - c->y - c->height + 1 : 0;
		uint64_t d = (uint64_t)dx * dx + (uint64_t)dy * dy;
		unsigned int key = c->group * 256u + c->number;
		if(d < distance || (d == distance && key < best_key))
		{ best = i; best_key = key; distance = d; }
	}
	const struct pgb_silver_connection *owner = &s->atlas[best];
	uint8_t block = owner->border;
	if(distance == 0)
	{
		block = s->rom[owner->blocks + (by - owner->y) * owner->width + bx - owner->x];
		/* Keep live map edits where RAM really covers a map rectangle,
		 * but never use its current-map padding as global ocean/forest. */
		if(bx >= 0 && by >= 0 && bx < map->width && by < map->height)
		{
			uint8_t live = s->frame_blocks[by * map->width + bx];
			if(live || (bx >= 3 && by >= 3 && bx < map->width - 3 && by < map->height - 3)) block = live;
		}
		if(!block) block = owner->border;
	}
	return block;
}

static uint8_t pgb_silver_world_tile(const struct pgb_silver_jp *s,
		const struct PGB_SILVER_STATE *gb, const struct pgb_silver_map *map, int x, int y)
{
	uint8_t block = pgb_silver_world_block(s, gb, map, x, y);
	return s->rom[map->definitions + (block & 127) * 16 +
		(((uint32_t)y & 31) / 8) * 4 + ((uint32_t)x & 31) / 8];
}

static int pgb_silver_map_state(const struct pgb_silver_jp *s,
		const struct PGB_SILVER_STATE *gb, struct pgb_silver_map *m)
{
	int anchor, bg_anchor, dx, dy;
	unsigned int bank, pointer;
	if(!s->supported || gb->wram[0x114b] != 2 || gb->wram[0x1108] != 0 ||
		(gb->hram_io[0x40] & 0x99) != 0x81)
		return 0; /* map handling, LCD/BG on, signed tiles, BG map 9800 */
	m->width = gb->wram[0x107a] + 6;
	m->height = gb->wram[0x1079] + 6;
	if(m->width <= 6 || m->height <= 6 || m->width * m->height > 1300 ||
		gb->wram[0x1071] > 1 || gb->wram[0x1072] > 1)
		return 0;
	anchor = pgb_silver_word(gb, 0xd06f) - 0xc700;
	bg_anchor = pgb_silver_word(gb, 0xd04d) - 0x9800;
	if(anchor < 0 || anchor >= m->width * m->height || bg_anchor < 0 || bg_anchor >= 1024)
		return 0;
	bank = gb->wram[0x10b7];
	pointer = pgb_silver_word(gb, 0xd0b8);
	if(bank == 0 || bank >= 64 || pointer < 0x4000 || pointer > 0x7ff0)
		return 0;
	m->definitions = bank * 0x4000u + pointer - 0x4000u;
	if(m->definitions + 2048 > s->rom_size)
		return 0;
	m->border = gb->wram[0x1078];
	/* The map anchor advances at step START, before SCX/SCY catch up.
	 * Relative displacement from the BG anchor retains the exact pixel phase,
	 * even while scrolling backwards and wrapping SCX/SCY through 255. */
	dx = (gb->hram_io[0x43] - (bg_anchor & 31) * 8) & 255;
	dy = (gb->hram_io[0x42] - (bg_anchor >> 5) * 8) & 255;
	if(dx >= 128) dx -= 256;
	if(dy >= 128) dy -= 256;
	if(dx < -32 || dx > 32 || dy < -32 || dy > 32)
		return 0;
	m->origin_x = (anchor % m->width) * 32 + gb->wram[0x1072] * 16 + dx;
	m->origin_y = (anchor / m->width) * 32 + gb->wram[0x1071] * 16 + dy;
	return 1;
}

/* Sprite data is read from this exact ROM, not duplicated as game assets.
 * Facings $01:404a, SpriteMovementData $01:4274, OverworldSprites $05:47b0,
 * SpriteMons $05:463b, MonMenuIcons $23:6780, IconPointers $23:687b.
 */
struct pgb_silver_part
{
	int x;
	uint32_t graphics; /* high bit selects ROM; points to the two row bytes */
	uint16_t order;
	uint8_t flags;
};





/* Pose of an offscreen object without advancing its movement/animation timer. */
static int pgb_silver_pose(unsigned int action, unsigned int direction, unsigned int frame)
{
	switch(action)
	{
	case 1: return (direction & 12) | ((frame & 1) ? ((frame >> 2) & 3) : 0);
	case 2: return (direction & 12) | ((frame >> 2) & 3);
	case 3: return (direction & 12) | ((frame >> 3) & 3);
	case 4: return direction & 12;
	case 6: return 16 + ((direction >> 2) & 3);
	case 7: return 21;
	case 8: return 20;
	case 9: return 23;
	case 10: return 0;
	case 11: return 24;
	case 12: return 22;
	case 13: return 0;
	default: return -1; /* intentionally invisible/flickering/temporary effects */
	}
}

static int pgb_silver_rom_graphics(const struct pgb_silver_jp *s,
		const struct PGB_SILVER_STATE *gb, unsigned int id, int variables, struct pgb_silver_object *out)
{
	if(!variables && id >= 0xf0) return 0;
	for(unsigned int depth = 0; id >= 0xf0 && depth < 4; depth++)
		id = gb->wram[0x1548 + id - 0xf0];
	if(id == 0 || id >= 0xf0) return 0;
	if(id < 0x80)
	{
		const uint8_t *desc = s->rom + 0x147b0 + (id - 1) * 6;
		unsigned int pointer = desc[0] | (desc[1] << 8);
		if(desc[3] >= 64 || pointer < 0x4000 || pointer >= 0x8000) return 0;
		out->graphics = desc[3] * 0x4000u + pointer - 0x4000;
		out->tiles = (desc[2] >> 4) | ((desc[2] & 15) << 4);
		if(desc[4] == 3 && out->facing < 16) out->facing = 0; /* still sprites have only a down pose */
	}
	else
	{
		unsigned int species, icon, pointer;
		if(id <= 0xa2) species = s->rom[0x1463b + id - 0x80];
		else if(id == 0xe0) species = gb->wram[0x1be1];
		else if(id == 0xe1) species = gb->wram[0x1c10];
		else return 0;
		if(species == 0 || (species > 251 && species != 253)) return 0;
		icon = species == 253 ? 28 : s->rom[0x8e780 + species - 1];
		if(icon >= 40) return 0;
		pointer = pgb_silver_rom_word(s, 0x8e87b + icon * 2);
		if(pointer < 0x4000 || pointer >= 0x8000) return 0;
		out->graphics = 0x23 * 0x4000u + pointer - 0x4000;
		out->tiles = 4;
		out->facing = 0;
	}
	return 1;
}

/* Enumerate 13 live objects, then up to 15 unspawned map objects. */
static int pgb_silver_object(const struct pgb_silver_jp *s, const struct PGB_SILVER_STATE *gb,
		const struct pgb_silver_map *map, unsigned int index, struct pgb_silver_object *out)
{
	const uint8_t *o;
	unsigned int id, flags2, priority;
	int facing;
	memset(out, 0, sizeof(*out));
	if(index < 13)
	{
		o = gb->wram + 0x11f0 + index * 40;
		if(o[0] == 0 || (o[4] & 1)) return 0; /* absent or explicitly invisible */
		if(o[1] < 16 && gb->wram[0x1538 + o[1]]) return 0;
		const struct pgb_silver_npc_memory *memory = pgb_silver_npc_memory(s, gb, o[1]);
		facing = o[13];
		if(facing == 255 && ((o[5] & 0x40) || o[9] <= 1))
			facing = memory ? memory->facing : pgb_silver_pose(o[11], o[8], o[12]);
		if(facing < 0 || facing >= 32) return 0;
		/* Sprite X/Y wrap at 256. Unwrap around its map coordinate, rather
		 * than treating a right-edge sprite as a phantom on the left edge. */
		pgb_silver_live_position(gb, map, o, &out->x, &out->y);
		out->facing = (uint8_t)facing;
		out->tile = o[2];
		out->map_index = o[1];
		out->live = 1;
		flags2 = o[5];
		priority = (flags2 & 1) ? 1 : (flags2 & 2) ? 3 : 2;
		out->order = (3 - priority) * 512 + index * 16;
		out->flags = flags2 & 0x90;
		out->grass = (flags2 & 8) ? 0x80 : 0;
		return 1;
	}
	index -= 12; /* map object 1..15; object 0 is the live player */
	if(index >= 16 || gb->wram[0x1538 + index]) return 0;
	o = gb->wram + 0x1438 + index * 16;
	if(o[1] == 0 || o[1] == 255 || o[4] >= 37) return 0;
	/* An existing live object always wins, including intentional invisibility. */
	if(o[0] < 13)
	{
		const uint8_t *live = gb->wram + 0x11f0 + o[0] * 40;
		if(live[0] && live[1] == index) return 0;
	}
	{
		const uint8_t *movement = s->rom + 0x4274 + o[4] * 6;
		if(movement[3] & 1) return 0;
		facing = pgb_silver_pose(movement[2], movement[1] * 4, 0);
		if(facing < 0) return 0;
		flags2 = movement[4];
	}
	out->x = (o[3] + 2) * 16 - map->origin_x;
	out->y = (o[2] + 2) * 16 - map->origin_y - 4;
	out->facing = (uint8_t)facing;
	out->map_index = index;
	{
		const struct pgb_silver_npc_memory *memory = pgb_silver_npc_memory(s, gb, index);
		if(memory)
		{
			out->x = memory->world_x - map->origin_x;
			out->y = memory->world_y - map->origin_y;
			out->facing = memory->facing;
		}
	}
	out->flags = flags2 & 0x90;
	out->order = 512 + (13 + index) * 16;
	/* Reuse a loaded sprite, including Pokemon icons, without writing VRAM. */
	for(id = 1; id < 12; id++)
		if(gb->wram[0x104f + id * 2] == o[1])
		{
			out->tile = gb->wram[0x1050 + id * 2];
			return 1;
		}
	return pgb_silver_rom_graphics(s, gb, o[1], 1, out);
}

/* Match complete exported OAM groups to a live object. Game logic can be a
 * few pixels/a pose ahead of the last DMA; using that exact OAM pose avoids
 * a visible seam where a sprite crosses the native screen boundary. */
static void pgb_silver_oam_pose(const struct pgb_silver_jp *s,
		const struct PGB_SILVER_STATE *gb, struct pgb_silver_object *object, uint64_t *used_oam)
{
	uint64_t best_mask = 0;
	int best = 24 * 24 + 1;
	struct pgb_silver_object result = *object;
	for(unsigned int facing = 0; facing < 32; facing++)
	{
		unsigned int at = pgb_silver_rom_word(s, 0x404a + facing * 2);
		unsigned int n = s->rom[at++];
		if(n == 0 || n > 16) continue;
		for(unsigned int start = 0; start + n <= 40; start++)
		{
			uint64_t mask = (((uint64_t)1 << n) - 1) << start;
			if(*used_oam & mask) continue;
			int px = object->x + pgb_silver_signed(gb->oam[start * 4 + 1] - 8 -
				pgb_silver_signed(s->rom[at + 1]) - object->x);
			int py = object->y + pgb_silver_signed(gb->oam[start * 4] - 16 -
				pgb_silver_signed(s->rom[at]) - object->y);
			int dx = px - object->x, dy = py - object->y;
			int distance;
			if(dx < -24 || dx > 24 || dy < -24 || dy > 24) continue;
			distance = dx * dx + dy * dy;
			unsigned int j;
			if(distance > 24 * 24 || distance >= best) continue;
			for(j = 0; j < n; j++)
			{
				const uint8_t *part = s->rom + at + j * 4;
				const uint8_t *oam = gb->oam + (start + j) * 4;
				unsigned int tile = ((part[2] & 4) ? 0 : object->tile) + part[3];
				unsigned int flags = ((part[2] | ((part[2] & 2) ? object->grass : 0)) & 0xf0) | object->flags;
				if(oam[0] == 160 || oam[0] != (uint8_t)(py + pgb_silver_signed(part[0]) + 16) ||
					oam[1] != (uint8_t)(px + pgb_silver_signed(part[1]) + 8) ||
					oam[2] != (uint8_t)tile || (oam[3] & 0xf0) != flags) break;
			}
			if(j == n)
			{
				best = distance;
				best_mask = mask;
				result.x = px; result.y = py;
				result.facing = (uint8_t)facing;
				result.order = start;
				result.oam_pose = 1;
			}
		}
	}
	*object = result;
	*used_oam |= best_mask;
}

/* Ordinary NPCs outside hardware OAM use immutable ROM graphics, avoiding
 * a blank frame while their VRAM slot is being (re)loaded. Player/effects and
 * published OAM sprites keep using the actual hardware graphics. */
static void pgb_silver_npc_graphics(const struct pgb_silver_jp *s,
		const struct PGB_SILVER_STATE *gb, struct pgb_silver_object *object)
{
	unsigned int id;
	if(object->oam_pose || object->map_index == 0 || object->map_index >= 16) return;
	id = gb->wram[0x1438 + object->map_index * 16 + 1];
	for(unsigned int depth = 0; id >= 0xf0 && depth < 4; depth++)
		id = gb->wram[0x1548 + id - 0xf0];
	if(id > 0 && id < 0x80)
	{
		const uint8_t *desc = s->rom + 0x147b0 + (id - 1) * 6;
		unsigned int pointer = desc[0] | (desc[1] << 8);
		if(desc[3] >= 64 || pointer < 0x4000 || pointer >= 0x8000) return;
		object->graphics = desc[3] * 0x4000u + pointer - 0x4000;
		object->tiles = (desc[2] >> 4) | ((desc[2] & 15) << 4);
	}
}

/* Raw sprite coordinates and the player camera offset are updated in
 * separate instructions, sometimes a game tick ahead of the visible BG/OAM.
 * Unpublished ordinary NPCs therefore need world coordinates, not that mixed
 * screen-space phase. Reconstruct the normal 16-pixel walk from its target
 * and remaining StepVectors ticks; keep special scripted motion on its raw
 * path. Published OAM remains authoritative at the native-screen seam. */
static void pgb_silver_npc_position(const struct PGB_SILVER_STATE *gb,
		const struct pgb_silver_map *map, unsigned int slot, struct pgb_silver_object *object)
{
	const uint8_t *o = gb->wram + 0x11f0 + slot * 40;
	int x = (o[16] + 2) * 16, y = (o[17] + 2) * 16 - 4;
	if(object->oam_pose || !object->map_index || object->map_index >= 16) return;
	if(o[7] == 255 && (o[9] < 8 || o[9] == 10 || o[9] == 11))
	{
		/* Sleep/standing/turn: no displacement from the map tile. */
	}
	else if(o[9] == 2 || o[9] == 7 || o[9] == 15)
	{
		unsigned int walking = o[7] & 15;
		int speed;
		if(walking >= 12) return;
		speed = 1 << (walking >> 2);
		if(o[10] > 16 / speed) return;
		switch(walking & 3)
		{
		case 0: y -= o[10] * speed; break;
		case 1: y += o[10] * speed; break;
		case 2: x += o[10] * speed; break;
		case 3: x -= o[10] * speed; break;
		}
	}
	else return; /* jumps, tracking, teleport, shakes and other scripted effects */
	object->x = x + pgb_silver_signed(o[25]) - map->origin_x;
	object->y = y + pgb_silver_signed(o[26]) - map->origin_y;
}

/* Return the ROM object-event array and its scripts-bank base. Every event
 * record is 13 bytes; runtime map objects prepend a slot ID and add padding. */
static unsigned int pgb_silver_region_events(const struct pgb_silver_jp *s,
		const struct pgb_silver_connection *region, uint32_t *records, uint32_t *base)
{
	unsigned int bank = s->rom[region->attributes + 6];
	unsigned int pointer = pgb_silver_rom_word(s, region->attributes + 9), at;
	if(!bank || bank >= 64 || pointer < 0x4000 || pointer >= 0x8000) return 0;
	*base = bank * 0x4000u - 0x4000u; at = *base + pointer + 2;
	const unsigned int sizes[] = {5,8,5};
	for(unsigned int i = 0; i < 3; i++)
	{
		if(at >= *base + 0x8000) return 0;
		at += 1 + s->rom[at] * sizes[i];
	}
	if(at >= *base + 0x8000) return 0;
	unsigned int count = s->rom[at++];
	if(count > 14 || at + count * 13 > *base + 0x8000) return 0;
	*records = at;
	return count;
}

static int pgb_silver_event_set(const struct PGB_SILVER_STATE *gb, unsigned int flag)
{
	return flag < 0x800 ? !!(gb->wram[0x17aa + flag / 8] & (1u << (flag & 7))) : 1;
}

static int pgb_silver_event_time(const struct PGB_SILVER_STATE *gb, const uint8_t *record)
{
	unsigned int first = record[5], last = record[6], hour = gb->hram_io[0x96];
	if(first == 255)
		return last == 255 || (gb->wram[0x1149] < 3 && (last & (1u << gb->wram[0x1149])));
	if(first == last) return 1;
	return first < last ? hour >= first && hour <= last : hour >= first || hour <= last;
}

/* Project only visibility onto a local bit mask. Never execute a callback in
 * the emulator or write flags, scripts, object RAM, VRAM, RTC or RNG. Unknown
 * bytecode returns no preview for that map rather than guessing story state.
 * Verified IDs: event flags $d7aa, EngineFlags $03:404d, weekday $d1e5.
 */
static int pgb_silver_preview_mask(const struct pgb_silver_jp *s, const struct PGB_SILVER_STATE *gb,
		const struct pgb_silver_connection *region, uint32_t records, unsigned int count,
		uint32_t base, uint16_t *mask)
{
	uint16_t visible = 0;
	for(unsigned int i = 0; i < count; i++)
	{
		const uint8_t *o = s->rom + records + i * 13;
		unsigned int flag = o[11] | (o[12] << 8);
		if(o[0] && pgb_silver_event_time(gb, o) && (flag == 65535 || !pgb_silver_event_set(gb, flag)))
			visible |= 1u << i;
	}
	unsigned int pointer = pgb_silver_rom_word(s, region->attributes + 7), at;
	if(pointer < 0x4000 || pointer >= 0x8000) return 0;
	at = base + pointer;
	at += 1 + s->rom[at] * 4;
	if(at >= base + 0x8000) return 0;
	unsigned int callbacks = s->rom[at++], pc = 0, value = 0;
	int known = 0;
	if(at + callbacks * 3 > base + 0x8000) return 0;
	for(unsigned int i = 0; i < callbacks; i++)
		if(s->rom[at + i * 3] == 2) { pc = pgb_silver_rom_word(s, at + i * 3 + 1); break; }
	if(!pc) { *mask = visible; return 1; }
	for(unsigned int step = 0; step < 64; step++)
	{
		if(pc < 0x4000 || pc > 0x7ffc) return 0;
		unsigned int op = s->rom[base + pc++], argument, jump, take = 0;
		switch(op)
		{
		case 0x8f: case 0x90: *mask = visible; return 1; /* endcallback/end */
		case 0x31: /* checkevent */
			argument = pgb_silver_rom_word(s, base + pc); pc += 2;
			if(argument >= 0x800) return 0;
			value = pgb_silver_event_set(gb, argument); known = 1; break;
		case 0x34: /* checkflag: read the ROM's engine-flag location/mask */
			argument = pgb_silver_rom_word(s, base + pc); pc += 2;
			if(argument >= 0x5d) return 0;
			jump = pgb_silver_rom_word(s, 0xc04d + argument * 3);
			if(jump < 0xc000 || jump >= 0xe000) return 0;
			value = !!(gb->wram[jump - 0xc000] & s->rom[0xc04f + argument * 3]); known = 1; break;
		case 0x1c: /* readvar VAR_WEEKDAY */
			if(s->rom[base + pc++] != 0x0b) return 0;
			value = gb->wram[0x11e5] % 7; known = 1; break;
		case 0x6d: case 0x6e: /* disappear/appear */
			argument = s->rom[base + pc++];
			if(argument < 2 || argument >= count + 2) return 0;
			if(op == 0x6d) visible &= ~(1u << (argument - 2));
			else visible |= 1u << (argument - 2);
			break;
		case 0x03: take = 1; goto branch; /* sjump */
		case 0x08: if(!known) return 0; take = !value; goto branch;
		case 0x09: if(!known) return 0; take = !!value; goto branch;
		case 0x06: case 0x07: case 0x0a: case 0x0b:
			if(!known) return 0;
			argument = s->rom[base + pc++];
			take = op == 6 ? value == argument : op == 7 ? value != argument :
				op == 10 ? value > argument : value < argument;
branch:
			jump = pgb_silver_rom_word(s, base + pc); pc = take ? jump : pc + 2; break;
		default: return 0;
		}
	}
	return 0;
}

static void pgb_silver_preview_sprites(struct pgb_silver_jp *s,
		const struct PGB_SILVER_STATE *gb, const struct pgb_silver_map *map)
{
	if(!(s->pad_x || s->pad_y) || !s->sprites_enabled ||
		!(s->features & PGB_SILVER_NEIGHBOR_NPCS)) return;
	int left = -((int)s->viewport_width - 160) / 2, top = -((int)s->viewport_height - 144) / 2;
	for(unsigned int region = 0; region < s->atlas_count; region++)
	{
		const struct pgb_silver_connection *c = &s->atlas[region];
		if(c->group == s->history_group && c->number == s->history_number) continue;
		int origin_x = c->x * 32 - map->origin_x, origin_y = c->y * 32 - map->origin_y;
		if(origin_x > left + s->viewport_width + 32 || origin_x + c->width * 32 < left - 32 ||
			origin_y > top + s->viewport_height + 32 || origin_y + c->height * 32 < top - 32) continue;
		uint32_t records = 0, base = 0;
		uint16_t visible;
		unsigned int count = pgb_silver_region_events(s, c, &records, &base);
		if(!count || !pgb_silver_preview_mask(s, gb, c, records, count, base, &visible)) continue;
		for(unsigned int i = 0; i < count; i++)
		{
			if(!(visible & (1u << i))) continue;
			const uint8_t *o = s->rom + records + i * 13;
			if(o[3] >= 37 || o[1] < 4 || o[2] < 4) continue;
			const uint8_t *movement = s->rom + 0x4274 + o[3] * 6;
			int facing = pgb_silver_pose(movement[2], movement[1] * 4, 0);
			if((movement[3] & 1) || facing < 0) continue;
			struct pgb_silver_object object = {0};
			object.x = origin_x + (o[2] - 4) * 16;
			object.y = origin_y + (o[1] - 4) * 16 - 4;
			if(object.x + 32 <= left || object.x - 16 >= left + s->viewport_width ||
				object.y + 32 <= top || object.y - 16 >= top + s->viewport_height) continue;
			object.facing = facing; object.flags = movement[4] & 0x90;
			object.map_index = i + 2; object.map_group = c->group; object.map_number = c->number;
			object.preview = 1; object.order = 2048 + s->object_count * 16;
			if(!pgb_silver_rom_graphics(s, gb, o[0], 0, &object)) continue;
			/* The live tileset collision table identifies tall grass under feet. */
			unsigned int tx = o[2] - 4, ty = o[1] - 4;
			if(tx < (unsigned int)c->width * 2 && ty < (unsigned int)c->height * 2)
			{
				unsigned int bank = gb->wram[0x10ba], ptr = pgb_silver_word(gb, 0xd0bb);
				unsigned int block = s->rom[c->blocks + (ty / 2) * c->width + tx / 2];
				if(bank > 0 && bank < 64 && ptr >= 0x4000 && ptr + block * 4 + 3 < 0x8000 &&
					s->rom[bank * 0x4000u + ptr - 0x4000 + block * 4 + (ty & 1) * 2 + (tx & 1)] == 0x18)
					object.grass = 0x80;
			}
			if(s->object_count < s->object_limit) s->objects[s->object_count++] = object;
			else s->objects_dropped++;
		}
	}
}

static void pgb_silver_prepare_sprites(struct pgb_silver_jp *s,
		const struct PGB_SILVER_STATE *gb, const struct pgb_silver_map *map)
{
	uint64_t used_oam = 0;
	if(s->history_group != gb->wram[0x19c6] || s->history_number != gb->wram[0x19c7])
	{
		memset(s->npc, 0, sizeof(s->npc));
		s->history_group = gb->wram[0x19c6];
		s->history_number = gb->wram[0x19c7];
	}
	for(unsigned int i = 1; i < 16; i++)
		if(gb->wram[0x1538 + i]) s->npc[i].valid = 0;
	s->object_count = 0; s->objects_dropped = 0;
	if(!(s->features & PGB_SILVER_FACING_MEMORY) &&
		(!(s->features & PGB_SILVER_SPRITES) || !s->sprites_enabled))
	{ s->sprite_ready = 1; return; }
	for(unsigned int index = 0; index < 28; index++)
	{
		struct pgb_silver_object object;
		if(!pgb_silver_object(s, gb, map, index, &object)) continue;
		if(index < 13)
		{
			pgb_silver_oam_pose(s, gb, &object, &used_oam);
			pgb_silver_npc_position(gb, map, index, &object);
		}
		if(object.live && object.map_index > 0 && object.map_index < 16)
		{
			struct pgb_silver_npc_memory *memory = &s->npc[object.map_index];
			const uint8_t *record = gb->wram + 0x1438 + object.map_index * 16;
			memory->identity = pgb_silver_npc_identity(record);
			memory->record_x = record[3];
			memory->record_y = record[2];
			memory->world_x = object.x + map->origin_x;
			memory->world_y = object.y + map->origin_y;
			memory->facing = object.facing;
			memory->direction = object.facing < 16 ? object.facing & 12 :
				gb->wram[0x11f0 + index * 40 + 8] & 12;
			memory->valid = 1;
		}
		object.map_group = s->history_group; object.map_number = s->history_number;
		pgb_silver_npc_graphics(s, gb, &object);
		s->objects[s->object_count++] = object;
	}
	pgb_silver_preview_sprites(s, gb, map);
	s->sprite_ready = 1;
}

static int pgb_silver_part_order(const void *aa, const void *bb)
{
	const struct pgb_silver_part *a = aa, *b = bb;
	if(a->x != b->x) return a->x < b->x ? -1 : 1;
	return (int)a->order - (int)b->order;
}

/* Draw a presentation scanline of OBJ pixels. No OAM, RAM or game state writes.
 * Current-map objects and bounded neighbour previews share the same priority.
 * Each object contributes at most four 8x8 pieces to a row.
 * Pixel priority matches DMG: smaller X, then original object/OAM order.
 */
static unsigned int pgb_silver_sprite_layer(const struct pgb_silver_jp *s,
		const struct PGB_SILVER_STATE *gb, const struct pgb_silver_map *map,
		uint8_t *pixels, uint16_t width, int x, int y, int previews_only)
{
	struct pgb_silver_part parts[PGB_SILVER_MAX_OBJECTS * 4];
	unsigned int count = 0, drawn = 0;
	if(!s->sprites_enabled || !(s->features & PGB_SILVER_SPRITES) || !(gb->hram_io[0x40] & 2)) return 0;
	for(unsigned int i = 0; i < s->object_count; i++)
	{
		struct pgb_silver_object object = s->objects[i];
		if(previews_only && !object.preview) continue;
		/* The first 32 ROM poses fit offsets [-8,24], plus an 8px tile.
		 * Reject whole objects before parsing their pieces on every row. */
		if(object.facing < 32 && (y < object.y - 8 || y >= object.y + 32 ||
			object.x + 32 <= x || object.x - 8 >= x + width)) continue;
		unsigned int address, n;
		address = pgb_silver_rom_word(s, 0x404a + object.facing * 2);
		if(address < 0x4000 || address >= 0x4274) continue;
		n = s->rom[address++];
		if(n > 16 || address + n * 4 > 0x4274) continue;
		for(unsigned int j = 0; j < n; j++, address += 4)
		{
			int py = object.y + pgb_silver_signed(s->rom[address]);
			int px = object.x + pgb_silver_signed(s->rom[address + 1]);
			unsigned int flags = s->rom[address + 2], tile = s->rom[address + 3];
			unsigned int row;
			uint32_t graphics;
			if(y < py || y >= py + 8 || px + 8 <= x || px >= x + width) continue;
			row = (unsigned int)(y - py);
			if(flags & 2) flags |= object.grass;
			flags = (flags & 0xf0) | object.flags;
			if(flags & 0x40) row = 7 - row;
			if(s->rom[address + 2] & 4)
				graphics = tile * 16; /* absolute tiles (emotes, fishing rod) */
			else if(object.graphics)
			{
				unsigned int frame_tile = tile >= 128 ? object.tiles + tile - 128 : tile;
				graphics = 0x80000000u | (object.graphics + frame_tile * 16);
			}
			else graphics = ((object.tile + tile) & 255) * 16;
			graphics += row * 2;
			if((graphics & 0x7fffffffu) + 1 >= ((graphics & 0x80000000u) ? s->rom_size : GX_GB_VRAM_SIZE)) continue;
			if(count >= sizeof(parts) / sizeof(*parts)) continue;
			parts[count].x = px;
			parts[count].graphics = graphics;
			parts[count].flags = (uint8_t)flags;
			parts[count++].order = object.order + j;
		}
	}
	qsort(parts, count, sizeof(*parts), pgb_silver_part_order);
	/* Paint lowest priority first. A higher priority OBJ hidden behind BG
	 * restores BG, so a lower priority OBJ cannot incorrectly show through. */
	while(count)
	{
		const struct pgb_silver_part *p = &parts[--count];
		const uint8_t *graphics = p->graphics & 0x80000000u ?
			s->rom + (p->graphics & 0x7fffffffu) : gb->vram + p->graphics;
		for(int column = 0; column < 8; column++)
		{
			int screen_x = p->x + column, dx = screen_x - x;
			unsigned int bit, colour;
			if(dx < 0 || dx >= width) continue;
			bit = (p->flags & 0x20) ? column : 7 - column;
			colour = ((graphics[0] >> bit) & 1) | (((graphics[1] >> bit) & 1) << 1);
			if(!colour) continue; /* OBJ colour 0 is transparent before palette mapping */
			if(p->flags & 0x80)
			{
				int wx = map->origin_x + screen_x, wy = map->origin_y + y;
				uint8_t tile = (screen_x >= 0 && screen_x < 160 && y >= 0 && y < 144) ?
					pgb_silver_tile(s, gb, map, wx, wy) : pgb_silver_world_tile(s, gb, map, wx, wy);
				unsigned int at = 0x800 + ((tile + 128) & 255) * 16 + ((uint32_t)wy & 7) * 2;
				unsigned int shift = 7 - ((uint32_t)wx & 7);
				unsigned int bg = ((gb->vram[at] >> shift) & 1) | (((gb->vram[at + 1] >> shift) & 1) << 1);
				if(bg)
				{
					pixels[dx] = gb->display.bg_palette[bg];
#if PGB_SILVER_LAYER_TAGS
					pixels[dx] |= GX_GB_PALETTE_BG;
#endif
					continue;
				}
			}
			pixels[dx] = gb->display.sp_palette[((p->flags & 0x10) ? 4 : 0) + colour];
#if PGB_SILVER_LAYER_TAGS
			pixels[dx] |= p->flags & 0x10;
#endif
			drawn++;
		}
	}
	return drawn;
}

static unsigned int pgb_silver_sprite_row(const struct pgb_silver_jp *s,
		const struct PGB_SILVER_STATE *gb, const struct pgb_silver_map *map,
		uint8_t *pixels, uint16_t width, int x, int y)
{
	return pgb_silver_sprite_layer(s, gb, map, pixels, width, x, y, 0);
}

#if !PGB_SILVER_LAYER_TAGS
/* Without layer tags, reconstruct native OBJ coverage from OAM to preserve
 * opaque player/NPC pixels, including palette colours equal to background. */
static void pgb_silver_native_coverage(const struct PGB_SILVER_STATE *gb, int y, uint8_t *mask)
{
	uint8_t selected[40];
	unsigned int count = 0, height = (gb->hram_io[0x40] & 4) ? 16 : 8;
	memset(mask, 0, 160);
	for(unsigned int i = 0; i < 40; i++)
	{
		int sy = (int)gb->oam[i * 4] - 16;
		if(y < sy || y >= sy + (int)height) continue;
#if PGB_SILVER_HIGH_ACCURACY
		unsigned int position = 0, key = gb->oam[i * 4 + 1] * 256u + i;
		while(position < count && gb->oam[selected[position] * 4 + 1] * 256u + selected[position] < key) position++;
		if(position >= 10) continue;
		if(count < 10) count++;
		for(unsigned int j = count - 1; j > position; j--) selected[j] = selected[j - 1];
		selected[position] = i;
#else
		selected[count++] = i;
#endif
	}
	for(unsigned int x = 0; x < 160; x++)
		for(unsigned int i = 0; i < count; i++)
		{
			const uint8_t *o = gb->oam + selected[i] * 4;
			int sx = (int)o[1] - 8, sy = (int)o[0] - 16;
			if(o[1] == 0 || o[1] >= 168 || (int)x < sx || (int)x >= sx + 8) continue;
			unsigned int py = y - sy, px = x - sx;
			if(o[3] & 0x40) py = height - 1 - py;
			if(o[3] & 0x20) px = 7 - px;
			unsigned int tile = height == 16 ? o[2] & 254 : o[2];
			unsigned int at = tile * 16 + py * 2, shift = 7 - px;
			unsigned int colour = ((gb->vram[at] >> shift) & 1) | (((gb->vram[at + 1] >> shift) & 1) << 1);
			if(!colour) continue;
			if(o[3] & 0x80)
			{
				uint8_t bx = x + gb->hram_io[0x43], by = y + gb->hram_io[0x42];
				unsigned int bg = gb->vram[0x1800 + (by >> 3) * 32 + (bx >> 3)];
				unsigned int offset = 0x800 + ((bg + 128) & 255) * 16 + (by & 7) * 2;
				unsigned int bit = 7 - (bx & 7);
				if(((gb->vram[offset] >> bit) & 1) | (((gb->vram[offset + 1] >> bit) & 1) << 1)) break;
			}
			mask[x] = 1; break;
		}
}
#endif

/* Post-compose neighbouring previews over the original 160x144 COPY. Without
 * this pass a future NPC would disappear at the old screen edge before the
 * game loads its map. The hardware LCD output and emulated PPU remain intact;
 * native player/NPC pixels always win. Call from a frontend's viewport sink. */
static unsigned int pgb_silver_jp_foreground(const struct pgb_silver_jp *s,
		const struct PGB_SILVER_STATE *gb, uint8_t *pixels, int y)
{
	uint8_t original[160];
	int present = 0;
	if(!(s->features & PGB_SILVER_SPRITES) || !s->frame_valid || !s->sprites_enabled || !(s->pad_x || s->pad_y) ||
		y < 0 || y >= 144 || !(gb->hram_io[0x40] & 2)) return 0;
	for(unsigned int i = 0; i < s->object_count; i++)
		if(s->objects[i].preview && s->objects[i].x > -32 && s->objects[i].x < 160 &&
			s->objects[i].y > y - 32 && s->objects[i].y < y + 32) { present = 1; break; }
	if(!present) return 0;
	memcpy(original, pixels, sizeof(original));
	unsigned int drawn = pgb_silver_sprite_layer(s, gb, &s->frame_map, pixels, 160, 0, y, 1);
#if PGB_SILVER_LAYER_TAGS
	for(unsigned int x = 0; x < 160; x++)
		if((original[x] & GX_GB_PALETTE_ALL) != GX_GB_PALETTE_BG) pixels[x] = original[x];
#else
	uint8_t mask[160];
	pgb_silver_native_coverage(gb, y, mask);
	for(unsigned int x = 0; x < 160; x++) if(mask[x]) pixels[x] = original[x];
#endif
	return drawn;
}

/* Latch the scene and its whole block map at the first rendered line.
 * The game updates anchors/VRAM incrementally during visible scanlines;
 * re-validating each row used to clear individual rows and flash white lines.
 * A new frame either draws all margins or blanks all margins. This snapshot
 * costs 1300 bytes, and never changes emulated state or PPU timing.
 */
/* Count discrepant BG samples; font replacements identify real menus rather
 * than a temporarily incomplete scrolling column. Map samples themselves can
 * use high tile IDs, so only low-terrain -> font mismatches count as text. */
static unsigned int pgb_silver_map_mismatches(const struct pgb_silver_jp *s,
		const struct PGB_SILVER_STATE *gb, const struct pgb_silver_map *map, unsigned int *text)
{
	unsigned int mismatches = 0;
	*text = 0;
	for(int sy = 8; sy < 144; sy += 24)
		for(int sx = 8; sx < 160; sx += 24)
		{
			uint8_t bx = (uint8_t)(sx + gb->hram_io[0x43]);
			uint8_t by = (uint8_t)(sy + gb->hram_io[0x42]);
			uint8_t expected = pgb_silver_tile(s, gb, map, map->origin_x + sx, map->origin_y + sy);
			uint8_t actual = gb->vram[0x1800 + (by >> 3) * 32 + (bx >> 3)];
			if(expected != actual)
			{
				mismatches++;
				*text += expected < 0x60 && actual >= 0x60;
			}
		}
	return mismatches;
}

static int pgb_silver_begin_frame(struct pgb_silver_jp *s, const struct PGB_SILVER_STATE *gb)
{
	struct pgb_silver_map map;
	int previous_valid = s->frame_valid, connection_transition = 0;
	unsigned int previous_objects = s->object_count;
	unsigned int text;
	s->frame_ready = 1;
	#if PGB_SILVER_BLOCK_CACHE
	s->block_cache_valid = 0;
#endif
	s->frame_valid = 0;
	s->sprite_ready = 0;
	s->object_count = 0;
	if(pgb_silver_map_state(s, gb, &map) &&
		pgb_silver_map_mismatches(s, gb, &map, &text) == 0)
	{
		s->frame_map = map;
		memcpy(s->frame_blocks, gb->wram + 0x700, sizeof(s->frame_blocks));
		if(s->features & PGB_SILVER_CONNECTIONS) pgb_silver_connections(s, gb, &map);
		else memset(s->connections, 0, sizeof(s->connections));
		if((s->features & PGB_SILVER_CONNECTIONS) &&
			(s->features & (PGB_SILVER_STABLE_BORDER | PGB_SILVER_NEIGHBOR_NPCS)))
			pgb_silver_atlas(s, gb, &map);
		else s->atlas_count = 0;
	}
	else
	{
		/* A transient update can also coincide with the first scanline.
		 * Keep the last valid SAME-map blocks, moving its camera by actual
		 * SCX/SCY, instead of flashing a blank frame. Hard transitions,
		 * battles, text and large camera reanchors never reuse old scenery. */
		int dx = pgb_silver_signed(gb->hram_io[0x43] - s->frame_scx);
		int dy = pgb_silver_signed(gb->hram_io[0x42] - s->frame_scy);
		/* Seamless connected-map loads briefly set MAPSTATUS_ENTER. Keep
		 * the old, complete scene until the new snapshot is coherent. Its
		 * connection descriptors establish continuity; warps/doors, menus
		 * and unrelated maps still cannot reuse the old scene. */
		if(gb->wram[0x114b] == 1)
			for(unsigned int side = 0; side < 4; side++)
			{
				const struct pgb_silver_connection *c = &s->connections[side];
				if(c->blocks && c->group == gb->wram[0x19c6] && c->number == gb->wram[0x19c7] &&
					(c->group != s->history_group || c->number != s->history_number))
					connection_transition = 1;
			}
		if(!previous_valid || (gb->wram[0x114b] != 2 && !connection_transition) || gb->wram[0x1108] ||
			(gb->wram[0xfdf] & 0x41) != 1 ||
			(gb->hram_io[0x40] & 0x99) != 0x81 ||
			(!connection_transition && (s->history_group != gb->wram[0x19c6] || s->history_number != gb->wram[0x19c7])) ||
			dx < -8 || dx > 8 || dy < -8 || dy > 8) return 0;
		s->frame_map.origin_x += dx;
		s->frame_map.origin_y += dy;
		if(pgb_silver_map_mismatches(s, gb, &s->frame_map, &text) > 21 || text) return 0;
	}
	if(connection_transition)
	{
		int dx = pgb_silver_signed(gb->hram_io[0x43] - s->frame_scx);
		int dy = pgb_silver_signed(gb->hram_io[0x42] - s->frame_scy);
		s->object_count = previous_objects;
		for(unsigned int i = 0; i < previous_objects; i++)
		{
			s->objects[i].x -= dx;
			s->objects[i].y -= dy;
		}
		s->sprite_ready = 1;
	}
	else pgb_silver_prepare_sprites(s, gb, &s->frame_map);
	s->frame_scx = gb->hram_io[0x43];
	s->frame_scy = gb->hram_io[0x42];
	s->frame_valid = 1;
	return 1;
}

/* Return 1 for a rendered frame, 0 for a wholly blanked scene. Automatic
 * LY wrap detection also handles interlace (first rendered line may be 1),
 * frame skip, and top/bottom-only extensions. Call begin_frame explicitly if
 * using this renderer without Peanut-GB's scanline viewport callbacks.
 */
static int pgb_silver_jp_background(struct pgb_silver_jp *s,
		const struct PGB_SILVER_STATE *gb, uint8_t *pixels, uint16_t width, int32_t x, int32_t y)
{
	const struct pgb_silver_map *map = &s->frame_map;
	if(!(s->features & PGB_SILVER_TERRAIN)) return 0;
	unsigned int i, line = gb->hram_io[0x44];
	if(!s->frame_ready || (line < GX_GB_HEIGHT && line < s->last_native_line))
		pgb_silver_begin_frame(s, gb);
	if(line < GX_GB_HEIGHT) s->last_native_line = line;
	if(!s->frame_valid) return 0;
	uint8_t tile = 0, world_block = 0;
	for(i = 0; i < width; i++)
	{
		int wx = map->origin_x + x + (int)i, wy = map->origin_y + y;
		if(i == 0 || (wx & 7) == 0 || x + (int)i == 0 || x + (int)i == 160)
		{
			if(x + (int)i >= 0 && x + (int)i < 160 && y >= 0 && y < 144)
				tile = pgb_silver_tile(s, gb, map, wx, wy);
			else
			{
				/* The world owner only changes on 32-pixel block boundaries. */
				if(i == 0 || (wx & 31) == 0 || x + (int)i == 160)
					{
#if PGB_SILVER_BLOCK_CACHE
					int bx = pgb_silver_floor32(wx), by = pgb_silver_floor32(wy);
					unsigned int slot = ((unsigned int)bx & 7) | (((unsigned int)by & 1) << 3);
					if(!(s->block_cache_valid & (1u << slot)) ||
						s->block_cache[slot].x != bx || s->block_cache[slot].y != by)
					{
						s->block_cache[slot].x = bx; s->block_cache[slot].y = by;
						s->block_cache[slot].block = pgb_silver_world_block(s, gb, map, wx, wy);
						s->block_cache_valid |= (uint16_t)(1u << slot);
					}
					world_block = s->block_cache[slot].block;
#else
					world_block = pgb_silver_world_block(s, gb, map, wx, wy);
#endif
				}
				tile = s->rom[map->definitions + (world_block & 127) * 16 +
					(((uint32_t)wy & 31) / 8) * 4 + ((uint32_t)wx & 31) / 8];
			}
		}
		unsigned int offset = 0x800 + ((tile + 128) & 255) * 16 +
			((uint32_t)wy & 7) * 2;
		unsigned int shift = 7 - ((uint32_t)wx & 7);
		uint8_t colour = ((gb->vram[offset] >> shift) & 1) |
			(((gb->vram[offset + 1] >> shift) & 1) << 1);
		pixels[i] = gb->display.bg_palette[colour];
#if PGB_SILVER_LAYER_TAGS
		pixels[i] |= GX_GB_PALETTE_BG;
#endif
	}
	pgb_silver_sprite_row(s, gb, map, pixels, width, x, y);
	return 1;
}
/* Viewport callback variant: the core overwrites the native 160x144
 * rectangle immediately afterwards. Avoid reconstructing those pixels or
 * sorting/drawing objects which would be discarded. The full-row API above
 * remains available for standalone renderers. */
static int pgb_silver_jp_margins(struct pgb_silver_jp *s,
        const struct PGB_SILVER_STATE *gb, uint8_t *pixels, uint16_t width, int32_t x, int32_t y)
{
    if(y < 0 || y >= 144 || x >= 160 || x + width <= 0)
        return pgb_silver_jp_background(s, gb, pixels, width, x, y);
    unsigned int left = x < 0 ? (unsigned int)-x : 0;
    unsigned int right = x + width > 160 ? (unsigned int)(160 - x) : width;
    int valid = 1;
    if(left) valid = pgb_silver_jp_background(s, gb, pixels, left, x, y);
    if(right < width)
        valid = pgb_silver_jp_background(s, gb, pixels + right, width - right, 160, y);
    return valid;
}
#endif
