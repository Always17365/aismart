#ifndef TENSORFLOW_LINK_HPP_INCLUDED
#define TENSORFLOW_LINK_HPP_INCLUDED

void tensorflow_link_kernels_avgpooling_op();
void tensorflow_link_kernels_bias_op();
void tensorflow_link_kernels_cast_op();
void tensorflow_link_kernels_concat_op();
void tensorflow_link_kernels_constant_op();
void tensorflow_link_kernels_conv_ops();
void tensorflow_link_kernels_conv_ops_using_gemm();
void tensorflow_link_kernels_cwise_op_add_1();
void tensorflow_link_kernels_cwise_op_div();
void tensorflow_link_kernels_cwise_op_floor();
void tensorflow_link_kernels_cwise_op_mul_1();
void tensorflow_link_kernels_cwise_op_rsqrt();
void tensorflow_link_kernels_cwise_op_sub();
void tensorflow_link_kernels_depthwise_conv_op();
void tensorflow_link_kernels_identity_op();
void tensorflow_link_kernels_lrn_op();
void tensorflow_link_kernels_matmul_op();
void tensorflow_link_kernels_maxpooling_op();
void tensorflow_link_kernels_no_op();
void tensorflow_link_kernels_pack_op();
void tensorflow_link_kernels_padding_fifo_queue_op();
void tensorflow_link_kernels_queue_ops();
void tensorflow_link_kernels_random_op();
void tensorflow_link_kernels_relu_op();
void tensorflow_link_kernels_reshape_op();
void tensorflow_link_kernels_resize_bilinear_op();
void tensorflow_link_kernels_shape_ops();
void tensorflow_link_kernels_softmax_op();
void tensorflow_link_kernels_split_op();
void tensorflow_link_kernels_strided_slice_op();
void tensorflow_link_ops_array_ops();
void tensorflow_link_ops_data_flow_ops();
void tensorflow_link_ops_image_ops();
void tensorflow_link_ops_math_ops();
void tensorflow_link_ops_nn_ops();
void tensorflow_link_ops_no_op();
void tensorflow_link_ops_random_ops();
void tensorflow_link_kernels_function_ops();
void tensorflow_link_ops_function_ops();
void tensorflow_link_ops_functional_ops();
void tensorflow_link_common_runtime_direct_session();
void tensorflow_link_common_runtime_threadpool_device_factory();

// Call tensorflow_link_modules in game_instance::app_tensorflow_link.
inline void tensorflow_link_modules()
{
	tensorflow_link_kernels_avgpooling_op();
	tensorflow_link_kernels_bias_op();
	tensorflow_link_kernels_cast_op();
	tensorflow_link_kernels_concat_op();
	tensorflow_link_kernels_constant_op();
	tensorflow_link_kernels_conv_ops();
	tensorflow_link_kernels_conv_ops_using_gemm();
	tensorflow_link_kernels_cwise_op_add_1();
	tensorflow_link_kernels_cwise_op_div();
	tensorflow_link_kernels_cwise_op_floor();
	tensorflow_link_kernels_cwise_op_mul_1();
	tensorflow_link_kernels_cwise_op_rsqrt();
	tensorflow_link_kernels_cwise_op_sub();
	tensorflow_link_kernels_depthwise_conv_op();
	tensorflow_link_kernels_identity_op();
	tensorflow_link_kernels_lrn_op();
	tensorflow_link_kernels_matmul_op();
	tensorflow_link_kernels_maxpooling_op();
	tensorflow_link_kernels_no_op();
	tensorflow_link_kernels_pack_op();
	tensorflow_link_kernels_padding_fifo_queue_op();
	tensorflow_link_kernels_queue_ops();
	tensorflow_link_kernels_random_op();
	tensorflow_link_kernels_relu_op();
	tensorflow_link_kernels_reshape_op();
	tensorflow_link_kernels_resize_bilinear_op();
	tensorflow_link_kernels_shape_ops();
	tensorflow_link_kernels_softmax_op();
	tensorflow_link_kernels_split_op();
	tensorflow_link_kernels_strided_slice_op();
	tensorflow_link_ops_array_ops();
	tensorflow_link_ops_data_flow_ops();
	tensorflow_link_ops_image_ops();
	tensorflow_link_ops_math_ops();
	tensorflow_link_ops_nn_ops();
	tensorflow_link_ops_no_op();
	tensorflow_link_ops_random_ops();
	tensorflow_link_kernels_function_ops();
	tensorflow_link_ops_function_ops();
	tensorflow_link_ops_functional_ops();
	tensorflow_link_common_runtime_direct_session();
	tensorflow_link_common_runtime_threadpool_device_factory();
}

#endif
