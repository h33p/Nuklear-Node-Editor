#pragma once

static void draw_color(struct node* cnode, struct nk_context* ctx)
{
	nk_layout_row_dynamic(ctx, 25, 1);
	nk_button_color(ctx, cnode->color);
	cnode->color.r = (nk_byte)nk_propertyi(ctx, "#R:", 0, cnode->color.r, 255, 1,1);
	cnode->color.g = (nk_byte)nk_propertyi(ctx, "#G:", 0, cnode->color.g, 255, 1,1);
	cnode->color.b = (nk_byte)nk_propertyi(ctx, "#B:", 0, cnode->color.b, 255, 1,1);
	cnode->color.a = (nk_byte)nk_propertyi(ctx, "#A:", 0, cnode->color.a, 255, 1,1);
}

//Was too lazy to remember how to draw a text label (just remembered as writing this comment) so just used nk_propertyi for the values
static void draw_info(struct node* cnode, struct nk_context* ctx)
{
	nk_layout_row_dynamic(ctx, 25, 2);
	nk_button_color(ctx, cnode->color);
	nk_button_color(ctx, cnode->color);
	nk_propertyi(ctx, "#ID:", 0, cnode->ID, 255, 1,1);
	nk_propertyi(ctx, "#Inp:", 0, cnode->input_count, 255, 1,1);
}
