% two-level 
area_rram_mux_nonisolate_two_level = 2:2:50;
pn_ratio = 1;
prog_pn_ratio = 1;
pullup_trans_size = 1;
for i=1:1:length(area_rram_mux_nonisolate_two_level)
  area_multiplexing = (2*i+1)*(trans_area(1)+ trans_area(prog_pn_ratio))+(ceil(sqrt(2*i)))*(trans_area(1)+ trans_area(prog_pn_ratio));
  area_pullup_trans = 0*(trans_area(pullup_trans_size)+ trans_area(pullup_trans_size));

  area_buf = (2*i + 1) * (trans_area(1)+ trans_area(1*pn_ratio));

  area_rram_mux_nonisolate_two_level(i) = area_multiplexing + area_buf + area_pullup_trans;
end
% Delay and power when VDD=0.5V 
rram_mux_nonisolate_two_level_0p5V = [
2, 1.3385e-11,4.143e-09,1.5055e-06,3.9285e-17;
4, 1.851e-11,1.195e-08,1.4025e-06,4.4105e-17;
6, 1.9075e-11,1.635e-08,1.3855e-06,4.5435e-17;
8, 1.9125e-11,1.974e-08,1.3925e-06,4.572e-17;
10, 1.956e-11,2.476e-08,1.3995e-06,4.6755e-17;
12, 1.956e-11,2.832e-08,1.403e-06,4.6865e-17;
14, 1.9595e-11,3.166e-08,1.409e-06,4.7155e-17;
16, 1.959e-11,3.723e-08,1.412e-06,4.726e-17;
18, 2.001e-11,3.933e-08,1.418e-06,4.831e-17;
20, 2.0005e-11,4.356e-08,1.4215e-06,4.842e-17;
22, 2.0045e-11,4.69e-08,1.427e-06,4.8715e-17;
24, 2.004e-11,5.032e-08,1.4295e-06,4.879e-17;
26, 2.039e-11,5.439e-08,1.436e-06,4.989e-17;
28, 2.039e-11,5.775e-08,1.4385e-06,4.996e-17;
30, 2.039e-11,6.208e-08,1.4415e-06,5.007e-17;
32, 2.041e-11,6.56e-08,1.4475e-06,5.038e-17;
34, 2.0435e-11,6.89e-08,1.4495e-06,5.045e-17;
36, 2.041e-11,7.311e-08,1.453e-06,5.056e-17;
38, 2.079e-11,7.627e-08,1.4585e-06,5.165e-17;
40, 2.079e-11,7.955e-08,1.4605e-06,5.1725e-17;
42, 2.079e-11,8.813e-08,1.464e-06,5.1835e-17;
44, 2.082e-11,8.721e-08,1.4705e-06,5.2145e-17;
46, 2.082e-11,9.05e-08,1.472e-06,5.222e-17;
48, 2.082e-11,9.376e-08,1.4745e-06,5.2295e-17;
50, 2.127e-11,9.804e-08,1.482e-06,5.339e-17;
];

% Delay and power when VDD=0.6V 
rram_mux_nonisolate_two_level_0p6V = [
2, 1.01435e-11,4.846e-09,2.766e-06,5.896e-17;
4, 1.405e-11,1.45e-08,2.489e-06,6.5255e-17;
6, 1.4315e-11,1.945e-08,2.478e-06,6.7005e-17;
8, 1.4345e-11,2.316e-08,2.5085e-06,6.791e-17;
10, 1.476e-11,2.811e-08,2.499e-06,6.8865e-17;
12, 1.476e-11,3.31e-08,2.5035e-06,6.8975e-17;
14, 1.463e-11,3.681e-08,2.533e-06,6.93e-17;
16, 1.4735e-11,4.181e-08,2.5165e-06,6.943e-17;
18, 1.5215e-11,4.545e-08,2.5285e-06,7.0845e-17;
20, 1.502e-11,5.045e-08,2.5405e-06,7.121e-17;
22, 1.527e-11,5.417e-08,2.5415e-06,7.1335e-17;
24, 1.527e-11,5.781e-08,2.544e-06,7.1415e-17;
26, 1.55e-11,6.268e-08,2.5695e-06,7.303e-17;
28, 1.5495e-11,6.631e-08,2.5725e-06,7.311e-17;
30, 1.55e-11,7.131e-08,2.576e-06,7.323e-17;
32, 1.553e-11,7.502e-08,2.5845e-06,7.3585e-17;
34, 1.553e-11,7.866e-08,2.5865e-06,7.3655e-17;
36, 1.553e-11,8.353e-08,2.591e-06,7.3775e-17;
38, 1.587e-11,8.722e-08,2.6215e-06,7.471e-17;
40, 1.587e-11,9.085e-08,2.6245e-06,7.479e-17;
42, 1.5865e-11,9.585e-08,2.628e-06,7.4905e-17;
44, 1.591e-11,9.956e-08,2.6465e-06,7.517e-17;
46, 1.591e-11,1.032e-07,2.6495e-06,7.5245e-17;
48, 1.5905e-11,1.068e-07,2.6515e-06,7.533e-17;
50, 1.664e-11,1.118e-07,2.6465e-06,7.685e-17;
];

% Delay and power when VDD=0.7V 
rram_mux_nonisolate_two_level_0p7V = [
2, 8.516e-12,5.971e-09,4.3885e-06,8.398e-17;
4, 1.2145e-11,1.78e-08,3.938e-06,9.1795e-17;
6, 1.262e-11,2.376e-08,3.9085e-06,9.378e-17;
8, 1.271e-11,2.815e-08,3.95e-06,9.4175e-17;
10, 1.2935e-11,3.41e-08,3.945e-06,9.619e-17;
12, 1.2935e-11,4.012e-08,3.9505e-06,9.6295e-17;
14, 1.296e-11,4.45e-08,3.963e-06,9.673e-17;
16, 1.296e-11,5.053e-08,3.9685e-06,9.6885e-17;
18, 1.332e-11,5.474e-08,4.043e-06,9.88e-17;
20, 1.332e-11,6.077e-08,4.0485e-06,9.8955e-17;
22, 1.328e-11,6.515e-08,3.9945e-06,9.9595e-17;
24, 1.328e-11,6.99e-08,3.9975e-06,9.969e-17;
26, 1.3615e-11,7.538e-08,4.08e-06,1.0134e-16;
28, 1.3615e-11,7.967e-08,4.084e-06,1.01535e-16;
30, 1.3625e-11,8.57e-08,4.089e-06,1.0139e-16;
32, 1.3595e-11,9.008e-08,4.0345e-06,1.02335e-16;
34, 1.3595e-11,9.437e-08,4.038e-06,1.0243e-16;
36, 1.3625e-11,1.004e-07,4.1155e-06,1.02485e-16;
38, 1.3865e-11,1.046e-07,4.086e-06,1.0428e-16;
40, 1.3865e-11,1.089e-07,4.0895e-06,1.04375e-16;
42, 1.3865e-11,1.149e-07,4.0945e-06,1.04475e-16;
44, 1.389e-11,1.193e-07,4.1055e-06,1.0497e-16;
46, 1.389e-11,1.236e-07,4.109e-06,1.05015e-16;
48, 1.3885e-11,1.279e-07,4.1125e-06,1.0511e-16;
50, 1.4155e-11,1.338e-07,4.1165e-06,1.0715e-16;
];