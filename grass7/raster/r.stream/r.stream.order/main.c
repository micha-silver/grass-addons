/****************************************************************************
 *
 * MODULE:			r.stream.order
 * AUTHOR(S):		Jarek Jasiewicz jarekj amu.edu.pl
 *							 
 * PURPOSE:			Calculate Strahler's and more streams hierarchy
 *							It use r.stream.extract or r.watershed output files: 
 * 							stream, direction, accumulation and elevation. 
 * 							The output are set of raster maps and vector file containing
 * 							addational stream attributes.
 *
 * COPYRIGHT:		(C) 2009,2010 by the GRASS Development Team
 *
 *							This program is free software under the GNU General Public 
 *							License (>=v2). Read the file COPYING that comes with GRASS
 *							for details.
 *
 *****************************************************************************/
#define MAIN
#include <grass/glocale.h>
#include "local_proto.h"

int nextr[9] = { 0, -1, -1, -1, 0, 1, 1, 1, 0 };
int nextc[9] = { 0, 1, 0, -1, -1, -1, 0, 1, 1 };


int main(int argc, char *argv[])
{

  IO input[] = {
			{"streams",YES,"output of r.stream.extract or r.watershed"},
			{"dirs",YES,"output of r.stream.extract or r.watershed"},
			{"elevation",NO,"any type digital elevation model"},
			{"accum",NO,"any type acc map created by r.watershed or used in r.stream.extract"}
		};
			
		/* add new basic rdering here and in local_vars.h declaration 
		 * it will be added to output without addational programing.
		 * Ordering functions are to be added in stream_order.c file.
		 * Addational operations may shall be done in common part sections
		 * derivative orders (like Scheideggers/Shreve) shall be added only 
		 * to table definition as a formula and to description file. */
	IO output[] = {
			{"strahler",NO,"Strahler's stream order"},
			{"horton",NO,"Original Hortons's stream order"},
			{"shreve",NO,"Shereve's stream magnitude"},
			{"hack",NO,"Hack's streams or Gravelius stream hierarchy"},
			{"topo",NO,"Topological dimension of streams"}
		};
  struct GModule *module;	/* GRASS module for parsing arguments */
    
	struct Option *opt_input[input_size];
	struct Option *opt_output[orders_size];
	struct Option *opt_swapsize;
	struct Option *opt_vector;
	struct Flag *flag_zerofill,	* flag_accum, *flag_segmentation;
	
	int output_num;		
	int num_seg; /* number of segments */
	int segmentation, zerofill;
	int i; /* iteration vars */
	int number_of_segs;
	int number_of_streams;
	char *in_streams=NULL, *in_dirs=NULL, *in_elev=NULL, *in_accum=NULL;
	char *out_vector=NULL;
	
  G_gisinit(argv[0]);		

  module = G_define_module();
  module->description =
	_("Calculate Strahler's and more streams hierarchy. Basic module for topological analysis of drainage network");
	G_add_keyword("Strahler Stream Order");
	G_add_keyword("Hack Streams");
	G_add_keyword("Stream network topology");
	G_add_keyword("Stream network geometry");
	G_add_keyword("Nework vectorisation");

		for (i=0;i<input_size;++i) {
	opt_input[i] = G_define_standard_option(G_OPT_R_INPUT);
  opt_input[i]->key = input[i].name;
  opt_input[i]->required = input[i].required;
  opt_input[i]->description = _(input[i].description);
		}
	
	opt_vector = G_define_standard_option(G_OPT_V_OUTPUT);
	opt_vector->key="vector";
	opt_vector->required = NO;
	opt_vector->description =_("OUTPUT vector file to write stream atributes");
	opt_vector->guisection=_("Output");

			for (i=0;i<orders_size;++i) {
	opt_output[i] = G_define_standard_option(G_OPT_R_OUTPUT);
  opt_output[i]->key = output[i].name;
  opt_output[i]->required = NO;
  opt_output[i]->description = _(output[i].description);
  opt_output[i]->guisection = _("Output");
		}
	
	opt_swapsize = G_define_option();
	opt_swapsize->key="memory";
	opt_swapsize->type = TYPE_INTEGER;
	opt_swapsize->answer = "300";
	opt_swapsize->description =_("Max memory used in memory swap mode (MB)");
	opt_swapsize->guisection=_("Optional");	

	flag_zerofill = G_define_flag();
  flag_zerofill->key = 'z';
  flag_zerofill->description = _("Create zero-valued background instead of NULL");

	flag_segmentation = G_define_flag();
  flag_segmentation->key = 'm';
  flag_segmentation->description = _("Use memory swap (operation is slow)");
  
  flag_accum = G_define_flag();
  flag_accum->key = 'a';
  flag_accum->description = _("Use flow accumulation to trace horton and hack models");
	
    if (G_parser(argc, argv))	/* parser */
	exit(EXIT_FAILURE);
	

	/* check output names */	
  zerofill=(flag_zerofill->answer != 0);
  segmentation=(flag_segmentation->answer != 0);
  use_accum = (flag_accum->answer != 0);
  use_vector = (opt_vector->answer!=NULL);
  
  number_of_segs = (int)atof(opt_swapsize->answer);
	number_of_segs < 32 ? (int)(32/0.12) : number_of_segs/0.12;

			if(use_vector) 
		if(!opt_input[o_elev]->answer || !opt_input[o_accum]->answer)
	G_fatal_error(_("To calculate vector file both accum and elev are required"));
			if(use_accum) 
		if(!opt_input[o_accum]->answer)
	G_fatal_error(_("with -a (use accumulation) accum map is required"));
	
			for (i=0;i<orders_size;++i) {
		if(!opt_output[i]->answer)
			continue;
		if (G_legal_filename(opt_output[i]->answer) < 0)
	G_fatal_error(_("<%s> is an illegal file name"), opt_output[i]->answer);
	output_num++;
			} /* end for */
		if (!output_num && !opt_vector->answer)
	G_fatal_error(_("You must select one or more output orders maps or insert the table name"));
  
    /* start */
  in_streams=opt_input[o_streams]->answer;
	in_dirs=opt_input[o_dirs]->answer;
	in_elev=opt_input[o_elev]->answer;
	in_accum=opt_input[o_accum]->answer;
	out_vector=opt_vector->answer;
	
	output_map_names=(char**)G_malloc(orders_size*sizeof(char*));
		for(i=0;i<orders_size;++i)
	output_map_names[i]	= opt_output[i]->answer;
	 
  nrows = Rast_window_rows();
  ncols = Rast_window_cols();
    
	/* ALL IN RAM VERSION */	
		if (!segmentation) {
	G_message("ALL IN RAM CALCULATION");
	MAP map_streams, map_dirs;
	CELL **streams, **dirs;

	ram_create_map(&map_streams,CELL_TYPE);
	ram_read_map(&map_streams,in_streams,1,CELL_TYPE);
	ram_create_map(&map_dirs,CELL_TYPE);
	ram_read_map(&map_dirs,in_dirs,1,CELL_TYPE);
	stream_init((int)map_streams.min,(int)map_streams.max);
	number_of_streams=(int)(map_streams.max+1);
	streams=(CELL**)map_streams.map;
	dirs=(CELL**)map_dirs.map;
	
	ram_stream_topology(streams,dirs,number_of_streams);
	
		if(out_vector || output_map_names[o_horton] || 
			output_map_names[o_hack] || output_map_names[o_topo])
	ram_stream_geometry(streams,dirs);

/* common part */
		if(use_vector) {
	stream_sample_map(in_elev, number_of_streams, 0);
	stream_sample_map(in_elev, number_of_streams, 1);
		}
		if(use_accum || use_vector)
	stream_sample_map(in_accum, number_of_streams, 2);

		if( output_map_names[o_strahler] || output_map_names[o_horton] || out_vector)
	strahler(all_orders[o_strahler]);

		if(output_map_names[o_horton] || out_vector)
	horton(all_orders[o_strahler],all_orders[o_horton], number_of_streams);

		if(output_map_names[o_shreve] || out_vector)
	shreve(all_orders[o_shreve]);	

		if(output_map_names[o_hack] || output_map_names[o_topo] || out_vector)
	hack(all_orders[o_hack],all_orders[o_topo],number_of_streams);
/* end of common part */

		if(out_vector)
	ram_create_vector(streams, dirs, out_vector, number_of_streams);
	
	ram_close_raster_order(streams, number_of_streams, zerofill);
	ram_release_map(&map_streams);
	ram_release_map(&map_dirs);
	/* end ram section */
}

	/* SEGMENTATION VERSION */
if (segmentation) {  
  G_message("MEMORY SWAP CALCULATION: MAY TAKE SOME TIME!");
  
	SEG map_streams, map_dirs;
	SEGMENT *streams, *dirs;
		
	seg_create_map(&map_streams,SROWS, SCOLS, number_of_segs, CELL_TYPE);
	seg_read_map(&map_streams,in_streams,1,CELL_TYPE);
	seg_create_map(&map_dirs,SROWS, SCOLS, number_of_segs, CELL_TYPE);
	seg_read_map(&map_dirs,in_dirs,1,CELL_TYPE);
	stream_init((int)map_streams.min,(int)map_streams.max);
	number_of_streams=(int)(map_streams.max+1);
	streams=&map_streams.seg;
	dirs=&map_dirs.seg;

	seg_stream_topology(streams,dirs, number_of_streams);

		if(out_vector || output_map_names[o_horton] || 
			output_map_names[o_hack] || output_map_names[o_topo])
	seg_stream_geometry(streams,dirs);

/* common part */
		if(use_vector) {
	stream_sample_map(in_elev, number_of_streams, 0);
	stream_sample_map(in_elev, number_of_streams, 1);
		}
		if(use_accum || use_vector)
	stream_sample_map(in_accum, number_of_streams, 2);

		if( output_map_names[o_strahler] || output_map_names[o_horton] || out_vector)
	strahler(all_orders[o_strahler]);

		if(output_map_names[o_horton] || out_vector)
	horton(all_orders[o_strahler],all_orders[o_horton], number_of_streams);

		if(output_map_names[o_shreve] || out_vector)
	shreve(all_orders[o_shreve]);	

		if(output_map_names[o_hack] || output_map_names[o_topo] || out_vector)
	hack(all_orders[o_hack],all_orders[o_topo],number_of_streams);	
/* end of common part */

		if(out_vector)
	seg_create_vector(streams,dirs, out_vector, number_of_streams);		
	
	seg_close_raster_order(streams, number_of_streams, zerofill);
	seg_release_map(&map_streams);
	seg_release_map(&map_dirs);
	/* end segmentation section */
}


/* free */
G_free(stream_attributes);
G_free(init_streams);
G_free(outlet_streams);
G_free(init_cells);

    exit(EXIT_SUCCESS);

}

