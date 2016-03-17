#include "nn_common.h"
#include <set>

void InitModel()
{
    init_const_dict["n2n"] = &graph;
	init_const_dict["subgraph_concat"] = &graph;	
	init_const_dict["max_pool"] = &graph;	
	const Dtype init_scale = 0.05;
	
	auto* n2nsum_param = add_const<Node2NodeMsgParam>(model, "n2n");	
	auto* subgconcat_param = add_const<SubgraphConcatParam>(model, "subgraph_concat");
	auto* node_pool_param = add_const<NodeMaxPoolParam>(model, "max_pool");
	
    auto* w_n2l = add_diff<LinearParam>(model, "input-node-to-latent", cfg::node_dim, cfg::conv_size, 0, init_scale);
    auto* p_node_conv = add_diff<LinearParam>(model, "linear-node-conv", cfg::conv_size, cfg::conv_size, 0, init_scale);
	auto* out_params = add_diff<LinearParam>(model, "outparam", cfg::conv_size, cfg::fp_len, 0, init_scale);
	auto* h1_weight = add_diff<LinearParam>(model, "h1_weight", cfg::fp_len * cfg::dna_len, cfg::n_hidden, 0, init_scale);
	auto* h2_weight = add_diff<LinearParam>(model, "h2_weight", cfg::n_hidden, 1, 0, init_scale);

	ILayer<mode, Dtype>* input_layer = cl<InputLayer>("input", gnn, {});
    auto* input_message = cl<ParamLayer>(gnn, {input_layer}, {w_n2l}); 
	auto* input_potential_layer = cl<ReLULayer>(gnn, {input_message}); 

	int lv = 0;
	ILayer<mode, Dtype>* cur_message_layer = input_potential_layer;
	while (lv < cfg::max_lv)
	{	
		lv++; 
		auto* n2npool = cl<ParamLayer>(gnn, {cur_message_layer}, {n2nsum_param});

		auto* node_linear = cl<ParamLayer>(gnn, {n2npool}, {p_node_conv});

		auto* merged_linear = cl<CAddLayer>(gnn, {node_linear, input_message});  

		cur_message_layer = cl<ReLULayer>(gnn, {merged_linear}); 
	}			
	
	auto* out_linear = cl<ParamLayer>(gnn, {cur_message_layer}, {out_params});		
	auto* reluact_fp = cl<ReLULayer>(gnn, {out_linear});

	auto* out_pool = cl<ParamLayer>(gnn, {reluact_fp}, {node_pool_param});
	auto* y_potential = cl<ParamLayer>(gnn, {out_pool}, {subgconcat_param});

	auto* hidden = cl<ParamLayer>(gnn, {y_potential}, {h1_weight});
	
	auto* reluact_out_nn = cl<ReLULayer>(gnn, {hidden}); 
	
	auto* output = cl<ParamLayer>("output", gnn, {reluact_out_nn}, {h2_weight});
	
	cl<MSECriterionLayer>("mse", gnn, {output});	
	cl<ABSCriterionLayer>("mae", gnn, {output}, PropErr::N);
}

int main(int argc, const char** argv)
{
	cfg::LoadParams(argc, argv);			

	GPUHandle::Init(cfg::dev_id);	

	LoadIndexes(cfg::train_idx_file, train_idx);
	LoadIndexes(cfg::test_idx_file, test_idx);
	LoadRawData(graph_data, labels);

	InitModel();

    MainLoop(); 

	GPUHandle::Destroy();
	return 0;
}
