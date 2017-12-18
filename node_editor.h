//Node Editor, GNU GPL v3 (Originally Public Domain)
//This is a heavily extended version of Nuklear's node editor demo, it fixes a small bug with the grid, cleaned up global state, includes unlinking, snapping the links, deleting nodes, nodes with infinite amount of links (actually limited to a set amount but don't talk about that) as well as custom draw functions for each node

#ifndef NDE_MAX_FUNCS
#define NDE_MAX_FUNCS 32
#endif

#ifndef LEN
#define LEN(a) (sizeof(a)/sizeof(a)[0])
#endif

#pragma once

#ifndef NDE_RETURN
#define NDE_RETURN struct nk_color
#endif

typedef void (*NodeDrawFn) (struct node*, struct nk_context*);
typedef NDE_RETURN (*NodeCalcFn) (struct node*, struct nk_context*);

struct node_functions
{
	NodeDrawFn draw;
	NodeCalcFn calc;
};

extern struct node_functions node_ftables[NDE_MAX_FUNCS];

#ifndef MAX_INPUTS
#define MAX_INPUTS 32
#endif

#ifndef NDE_CUSTOM_DATA
struct node_data {
	struct nk_color color;

	node_data()
	{
		color = nk_rgb(255, 0, 0);
	}
};
#endif

struct node {
	int ID;
	char name[32];
	struct nk_rect bounds;
	float value;
	struct node_data data;
	struct node_link* inputs[MAX_INPUTS];
	struct nk_color gapped_color;
	int input_gapped;
	int min_input_count;
	int input_count;
	bool infinite_inputs;
	int output_count;
	struct node *next;
	struct node *prev;

	node_functions ftable;
};

struct node_link {
	int input_id;
	int input_slot;
	int output_id;
	int output_slot;
	int id;
	struct nk_vec2 in;
	struct nk_vec2 out;
};

struct node_linking {
	int active;
	struct node *node;
	int input_id;
	int input_slot;
	int output_id;
	int output_slot;
};

struct node_editor {
	int initialized;
	struct node node_buf[64];
	struct node_link links[256];
	struct node *begin;
	struct node *end;
	struct node *deleted_begin;
	struct node *deleted_end;
	struct node *hovered;
	bool popupOpened;
	int node_count;
	int link_count;
	struct nk_rect bounds;
	struct node *selected;
	int show_grid;
	struct nk_vec2 scrolling;
	struct node_linking linking;
};

static void node_editor_push(struct node_editor *editor, struct node *node, bool delete_list = false)
{
	struct node** begin;
	struct node** end;
	if (delete_list)
	{
		begin = &editor->deleted_begin;
		end = &editor->deleted_end;
	}
	else
	{
		begin = &editor->begin;
		end = &editor->end;
	}
	if (!*begin) {
		node->next = NULL;
		node->prev = NULL;
		*begin = node;
		*end = node;
	} else {
		node->prev = *end;
		if (*end)
			(*end)->next = node;
		node->next = NULL;
		*end = node;
	}
}

static struct node* node_editor_pop(struct node_editor *editor, struct node *node, bool delete_list = false)
{
	struct node** begin;
	struct node** end;
	if (delete_list)
	{
		begin = &editor->deleted_begin;
		end = &editor->deleted_end;
	}
	else
	{
		begin = &editor->begin;
		end = &editor->end;
	}
	if (node->next)
		node->next->prev = node->prev;
	if (node->prev)
		node->prev->next = node->next;
	if (*end == node)
		*end = node->prev;
	if (*begin == node)
		*begin = node->next;
	node->next = NULL;
	node->prev = NULL;
	return node;
}

static struct node* node_editor_find(struct node_editor *editor, int ID)
{
	struct node *iter = editor->begin;
	while (iter) {
		if (iter->ID == ID)
			return iter;
		iter = iter->next;
	}
	return NULL;
}

static void node_editor_add(struct node_editor *editor, const char *name, struct nk_rect bounds,
		struct node_data data, int in_count, int out_count, node_functions ftable, bool infinite_inputs = false, int gapped_inputs = 0, struct nk_color gapped_color = nk_rgb(100, 70, 70))
{
	struct node *node;
	if (!editor->deleted_begin)
	{
		if((nk_size)editor->node_count >= LEN(editor->node_buf)) return;
		node = &editor->node_buf[editor->node_count++];
		node->ID = editor->node_count - 1;
	}
	else
		node = node_editor_pop(editor, editor->deleted_begin, true);
	node->data = data;
	node->infinite_inputs = infinite_inputs;
	node->input_count = in_count;
	node->gapped_color = gapped_color;
	node->input_gapped = gapped_inputs;
	node->min_input_count = in_count;
	node->output_count = out_count;
	node->bounds = bounds;
	node->ftable = ftable;
	strcpy(node->name, name);
	node_editor_push(editor, node);
}

static void node_editor_link(struct node_editor *editor, int in_id, int in_slot,
	int out_id, int out_slot, bool uselinking = false)
{
	struct node_link* link = NULL;
	struct node *node = node_editor_find(editor, out_id);
	if (!node)
		return;
	if (uselinking && editor->linking.output_id == out_id)
	{
		node_link* temp = node->inputs[editor->linking.output_slot];
		if (temp)
			temp->output_slot = out_slot;
		node->inputs[editor->linking.output_slot] = node->inputs[out_slot];
		if (node->inputs[out_slot])
			node->inputs[out_slot]->output_slot = editor->linking.output_slot;
		node->inputs[out_slot] = temp;
	}
	if (node->inputs[out_slot] == NULL)
	{
		for (int i = 0; i < editor->link_count; i++)
			if (editor->links[i].output_id < 0 || editor->links[i].input_id < 0)
			{
				link = &editor->links[i];
				break;
			}
		if (!link)
		{
			if((nk_size)editor->link_count >= LEN(editor->links)) return;
			link = &editor->links[editor->link_count];
			link->id = editor->link_count++;
		}
	}
	else
		link = node->inputs[out_slot];
	link->input_id = in_id;
	link->input_slot = in_slot;
	link->output_id = out_id;
	link->output_slot = out_slot;
	node->inputs[out_slot] = link;
	if (node->infinite_inputs && out_slot >= node->input_count)
		node->input_count = out_slot + 1;
}

//I have a feeling the loop inside this function can be optimized
static void node_editor_clear_gaps(struct node_editor* editor)
{
	for (int i = 0; i < editor->node_count; i++)
	{
		struct node* node = &editor->node_buf[i];
		if (!node->infinite_inputs)
			continue;
		//TC needed to prevent the loop from staying infinite
		int tc = 0;
		for (int o = node->input_gapped; o < node->input_count; o++)
		{
			if (tc < node->input_count && (!node->inputs[o] || node->inputs[o]->output_id < 0 || node->inputs[o]->input_id < 0))
			{
				for(int u = o + 1; u < node->input_count; u++)
				{
					node->inputs[u - 1] = node->inputs[u];
					if (node->inputs[u] && node->inputs[u]->output_id >= 0 && node->inputs[u]->input_id >= 0)
						node->inputs[u]->output_slot--;
					node->inputs[u] = NULL;
				}
				o--;
				tc++;
			}
			else
				tc = o;
		}
		node->input_count = node->min_input_count - 1;
		if (node->input_count < node->input_gapped)
			node->input_count = node->input_gapped;
		if (node->input_count < 0)
			node->input_count = 0;
		while(node->input_count < MAX_INPUTS && node->inputs[node->input_count] && node->inputs[node->input_count]->output_id >= 0 && node->inputs[node->input_count]->input_id >= 0)
			node->input_count++;
	}
}

static void node_editor_clean_links(struct node_editor* editor)
{
	struct node* tnode = editor->begin;
	do
	{
		if (!tnode)
			break;
		for (int o = 0; o < (tnode->infinite_inputs ? MAX_INPUTS : tnode->input_count); o++)
			if (!tnode->inputs[o] || !node_editor_find(editor, tnode->inputs[o]->input_id))
			{
				if (tnode->inputs[o])
					tnode->inputs[o]->output_id = -1;
				tnode->inputs[o] = NULL;
			}
		tnode = tnode->next;
	} while (tnode);

	tnode = editor->deleted_begin;
	do
	{
		if (!tnode)
			break;
		for (int o = 0; o < (tnode->infinite_inputs ? MAX_INPUTS : tnode->input_count); o++)
		{
			if (tnode->inputs[o])
				tnode->inputs[o]->output_id = -1;
			tnode->inputs[o] = NULL;
		}
		tnode = tnode->next;
	} while (tnode);
}

#ifndef NDE_CUSTOM_INIT
static void node_editor_init(struct node_editor *editor)
{
	memset(editor, 0, sizeof(*editor));
	editor->begin = NULL;
	editor->end = NULL;
	editor->deleted_begin = NULL;
	editor->deleted_end = NULL;
	node_editor_add(editor, "Source", nk_rect(40, 10, 180, 220), node_data(), 0, 1, node_ftables[0]);
	node_editor_add(editor, "Source", nk_rect(40, 260, 180, 220), node_data(), 0, 1, node_ftables[0]);
	node_editor_add(editor, "Combine", nk_rect(400, 100, 180, 220), node_data(), 2, 2, node_ftables[0]);
	node_editor_link(editor, 0, 0, 2, 0);
	node_editor_link(editor, 1, 0, 2, 1);
	editor->show_grid = nk_true;
}
#endif

int node_edit(struct nk_context *ctx, struct node_editor* nodeedit, const char* title)
{
	int n = 0;
	struct nk_rect total_space;
	const struct nk_input *in = &ctx->input;
	struct nk_command_buffer *canvas;
	struct node *updated = 0;

	if (!nodeedit->initialized) {
		node_editor_init(nodeedit);
		nodeedit->initialized = 1;
	}

#ifndef NDE_NO_WINDOW
	if (nk_begin(ctx, title, nk_rect(0, 0, 800, 600),
				NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_MOVABLE|NK_WINDOW_CLOSABLE))
	{
#endif
		/* allocate complete window space */
		canvas = nk_window_get_canvas(ctx);
		total_space = nk_window_get_content_region(ctx);
		nk_layout_space_begin(ctx, NK_STATIC, total_space.h, nodeedit->node_count);
		{
			struct node *it = nodeedit->begin;
			struct nk_rect size = nk_layout_space_bounds(ctx);

			bool drawLink = false;
			struct nk_vec2 link_l0;
			struct nk_vec2 link_l1 = in->mouse.pos;

			if (nodeedit->show_grid) {
				/* display grid */
				float x, y;
				const float grid_size = 32.0f;
				const float grid_size2 = 160.0f;
				const struct nk_color grid_color = nk_rgb(50, 50, 50);
				for (x = (float)fmod(size.x - nodeedit->scrolling.x, grid_size); x < size.x + size.w; x += grid_size)
					nk_stroke_line(canvas, x, size.y, x, size.y+size.h, 1.0f, grid_color);
				for (y = (float)fmod(size.y - nodeedit->scrolling.y, grid_size); y < size.y + size.h; y += grid_size)
					nk_stroke_line(canvas, size.x, y, size.x+size.w, y, 1.0f, grid_color);
				for (x = (float)fmod(size.x - nodeedit->scrolling.x, grid_size2); x < size.x + size.w; x += grid_size2)
					nk_stroke_line(canvas, x, size.y, x, size.y+size.h, 2.0f, grid_color);
				for (y = (float)fmod(size.y - nodeedit->scrolling.y, grid_size2); y < size.y + size.h; y += grid_size2)
					nk_stroke_line(canvas, size.x, y, size.x+size.w, y, 2.0f, grid_color);
			}

			/* draw each link */
			for (n = 0; n < nodeedit->link_count; ++n) {
				struct node_link *link = &nodeedit->links[n];
				struct node *ni = node_editor_find(nodeedit, link->input_id);
				struct node *no = node_editor_find(nodeedit, link->output_id);
				if (!ni || !no)
					continue;
				float spacei = ni->bounds.h / (float)((ni->output_count) + 1);
				int inputs = (no->infinite_inputs && nodeedit->linking.active && nodeedit->linking.node != no && no->input_count < MAX_INPUTS) ? no->input_count + 1 : no->input_count;
				float spaceo = no->bounds.h / (float)((inputs) + 1);
				struct nk_vec2 l0 = nk_layout_space_to_screen(ctx,
						nk_vec2(ni->bounds.x + ni->bounds.w, 3.0f + ni->bounds.y + spacei * (float)(link->input_slot+1)));
				struct nk_vec2 l1 = nk_layout_space_to_screen(ctx,
						nk_vec2(no->bounds.x, 3.0f + no->bounds.y + spaceo * (float)(link->output_slot+1)));

				l0.x -= nodeedit->scrolling.x;
				l0.y -= nodeedit->scrolling.y;
				l1.x -= nodeedit->scrolling.x;
				l1.y -= nodeedit->scrolling.y;
				nk_stroke_curve(canvas, l0.x, l0.y, l0.x + 50.0f, l0.y,
						l1.x - 50.0f, l1.y, l1.x, l1.y, 1.0f, nk_rgb(100, 100, 100));
			}

			/* execute each node as a movable group */
			struct nk_panel *node;
			while (it) {
				/* calculate scrolled node window position and size */
				nk_layout_space_push(ctx, nk_rect(it->bounds.x - nodeedit->scrolling.x,
							it->bounds.y - nodeedit->scrolling.y, it->bounds.w, it->bounds.h));

				/* execute node window */
				if (nk_group_begin(ctx, it->name, NK_WINDOW_MOVABLE|NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER|NK_WINDOW_TITLE))
				{
					/* always have last selected node on top */

					node = nk_window_get_panel(ctx);
					if (nk_input_mouse_clicked(in, NK_BUTTON_LEFT, node->bounds) &&
							(!(it->prev && nk_input_mouse_clicked(in, NK_BUTTON_LEFT,
																										nk_layout_space_rect_to_screen(ctx, node->bounds)))) &&
							nodeedit->end != it)
					{
						updated = it;
					}
					/* contextual menu */
					if (!nodeedit->popupOpened && nk_input_is_mouse_hovering_rect(in, node->bounds))
						nodeedit->hovered = it;

					/* ================= NODE CONTENT =====================*/
					it->ftable.draw(it, ctx);
					/* ====================================================*/
					nk_group_end(ctx);
				}
				{
					/* node connector and linking */
					float space;
					struct nk_rect bounds;
					bounds = nk_layout_space_rect_to_local(ctx, node->bounds);
					bounds.x += nodeedit->scrolling.x;
					bounds.y += nodeedit->scrolling.y;
					it->bounds = bounds;

					/* output connector */
					space = node->bounds.h / (float)((it->output_count) + 1);
					for (n = 0; n < it->output_count; ++n) {
						struct nk_rect circle;
						circle.x = node->bounds.x + node->bounds.w-4;
						circle.y = node->bounds.y + space * (float)(n+1);
						circle.w = 8; circle.h = 8;
						nk_fill_circle(canvas, circle, nk_rgb(100, 100, 100));

						/* start linking process */
						if (nk_window_is_active(ctx, title) && nk_input_has_mouse_click_down_in_rect(in, NK_BUTTON_LEFT, circle, nk_true)) {
							nodeedit->linking.active = nk_true;
							nodeedit->linking.node = it;
							nodeedit->linking.input_id = it->ID;
							nodeedit->linking.input_slot = n;
							nodeedit->linking.output_id = -1;
						}

						if (nodeedit->linking.active && nodeedit->linking.node == it &&
								nodeedit->linking.input_slot == n) {
							link_l0 = nk_vec2(circle.x + 3, circle.y + 3);
							drawLink = true;
						}
					}

					int inputs = (it->infinite_inputs && nodeedit->linking.active && nodeedit->linking.node != it && it->input_count < MAX_INPUTS) ? it->input_count + 1 : it->input_count;
					/* input connector */
					space = node->bounds.h / (float)(inputs + 1);
					for (n = 0; n < inputs; ++n) {
						struct nk_rect circle;
						circle.x = node->bounds.x-4;
						circle.y = node->bounds.y + space * (float)(n+1);
						circle.w = 8; circle.h = 8;
						nk_fill_circle(canvas, circle, n < it->input_gapped ? it->gapped_color :  nk_rgb(100, 100, 100));

						/* start linking process */
						if (nk_window_is_active(ctx, title) && nk_input_has_mouse_click_down_in_rect(in, NK_BUTTON_LEFT, circle, nk_true) && it->inputs[n]) {
							struct node* target = node_editor_find(nodeedit, it->inputs[n]->input_id);
							if (target) {
								nodeedit->linking.active = nk_true;
								nodeedit->linking.node = target;
								nodeedit->linking.input_id = target->ID;
								nodeedit->linking.input_slot = it->inputs[n]->input_slot;
								nodeedit->linking.output_id = it->ID;
								nodeedit->linking.output_slot = n;
								it->inputs[n]->input_id = -1;
							}
						}
						if (nk_input_is_mouse_hovering_rect(in, circle) && (!nodeedit->linking.active || nodeedit->linking.node != it))
							link_l1 = nk_vec2(circle.x + 3, circle.y + 3);

						if (nk_input_is_mouse_released(in, NK_BUTTON_LEFT) &&
								nk_input_is_mouse_hovering_rect(in, circle) &&
								nodeedit->linking.active && nodeedit->linking.node != it) {
							nodeedit->linking.active = nk_false;
							node_editor_link(nodeedit, nodeedit->linking.input_id,
									nodeedit->linking.input_slot, it->ID, n, true);
							node_editor_clear_gaps(nodeedit);
						}
					}
				}
				it = it->next;
			}


			/* draw curve from linked node slot to mouse position */
			if (drawLink)
				nk_stroke_curve(canvas, link_l0.x, link_l0.y, link_l0.x + 50.0f, link_l0.y,
						link_l1.x - 50.0f, link_l1.y, link_l1.x, link_l1.y, 1.0f, nk_rgb(100, 100, 100));

			/* reset linking connection */
			if (nodeedit->linking.active && nk_input_is_mouse_released(in, NK_BUTTON_LEFT)) {
				nodeedit->linking.active = nk_false;
				nodeedit->linking.node = NULL;
				node_editor_clear_gaps(nodeedit);
				fprintf(stdout, "linking failed\n");
			}

			if (updated) {
				/* reshuffle nodes to have least recently selected node on top */
				node_editor_pop(nodeedit, updated);
				node_editor_push(nodeedit, updated);
			}

			/* node selection */
			if (nk_window_is_active(ctx, title) && nk_input_mouse_clicked(in, NK_BUTTON_LEFT, nk_layout_space_bounds(ctx))) {
				it = nodeedit->begin;
				nodeedit->selected = NULL;
				nodeedit->bounds = nk_rect(in->mouse.pos.x, in->mouse.pos.y, 100, 200);
				while (it) {
					struct nk_rect b = nk_layout_space_rect_to_screen(ctx, it->bounds);
					b.x -= nodeedit->scrolling.x;
					b.y -= nodeedit->scrolling.y;
					if (nk_input_is_mouse_hovering_rect(in, b))
						nodeedit->selected = it;
					it = it->next;
				}
			}

			/* contextual menu */
			if (!nodeedit->hovered && nk_window_is_active(ctx, title) && nk_contextual_begin(ctx, 0, nk_vec2(100, 220), nk_window_get_bounds(ctx))) {
				nodeedit->popupOpened = true;
				const char *grid_option[] = {"Show Grid", "Hide Grid"};
				nk_layout_row_dynamic(ctx, 25, 1);
				if (nk_contextual_item_label(ctx, "New", NK_TEXT_CENTERED))
					node_editor_add(nodeedit, "New", nk_rect(400, 260, 180, 220),
									node_data(), 1, 2, node_ftables[1], true, 1);
				if (nk_contextual_item_label(ctx, grid_option[nodeedit->show_grid],NK_TEXT_CENTERED))
					nodeedit->show_grid = !nodeedit->show_grid;
				nk_contextual_end(ctx);
			}
			else if (nk_window_is_active(ctx, title) && nk_contextual_begin(ctx, 0, nk_vec2(100, 220), nk_window_get_bounds(ctx))) {
				nodeedit->popupOpened = true;
				nk_layout_row_dynamic(ctx, 25, 1);
				if (nk_contextual_item_label(ctx, "Delete", NK_TEXT_CENTERED))
				{
					node_editor_pop(nodeedit, nodeedit->hovered);
					node_editor_push(nodeedit, nodeedit->hovered, true);
					node_editor_clean_links(nodeedit);
					node_editor_clear_gaps(nodeedit);
				}
				nk_contextual_end(ctx);
			}
			else if (nk_window_is_active(ctx, title))
			{
				nodeedit->hovered = NULL;
				nodeedit->popupOpened = false;
			}
		}
		nk_layout_space_end(ctx);

		/* window content scrolling */
		if (nk_window_is_active(ctx, title) &&
				nk_input_is_mouse_down(in, NK_BUTTON_RIGHT)) {
			nodeedit->scrolling.x += in->mouse.delta.x;
			nodeedit->scrolling.y += in->mouse.delta.y;
		}
#ifndef NDE_NO_WINDOW
	}
	nk_end(ctx);
	return !nk_window_is_closed(ctx, title);
#else
	return 1;
#endif
}

