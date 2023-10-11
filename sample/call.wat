(module
  (func $add (param $p1 i32) (param $p2 i32) (result i32)
    (i32.add (local.get $p1) (local.get $p2))
  )

  (func (export "add42") (param $param i32) (result i32)
    (call $add (local.get $param) (i32.const 42))
  )
)