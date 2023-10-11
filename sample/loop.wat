(module
  (func (export "loop") (result i32)
    (local $i i32)
    (local $sum i32)

    (local.set $sum (i32.const 0))
    (local.set $i (i32.const 0))
    (block $block (loop $loop
      (br_if $block (i32.ge_s (local.get $i) (i32.const 3)))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (local.set $sum (i32.add (local.get $sum) (i32.const 14)))
      (br $loop)
    ))
    (local.get $sum)
  )
)