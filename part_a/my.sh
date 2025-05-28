tasks=(
  "super_super_light"
  "super_light"
  "ping_pong_equal"
  "ping_pong_unequal"
  "recursive_fibonacci"
  "math_operations_in_tight_for_loop"
  "math_operations_in_tight_for_loop_fewer_tasks"
  "math_operations_in_tight_for_loop_fan_in"
  "math_operations_in_tight_for_loop_reduction_tree"
  "spin_between_run_calls"
  "mandelbrot_chunked"
)

for task in "${tasks[@]}"; do
  ./runtasks -n 16 "$task"
done