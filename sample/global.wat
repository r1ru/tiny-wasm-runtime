(module
    (global $g (mut i32) (i32.const 5) )
    (func (export "set_get_global") (result i32)
        (global.set $g (i32.mul (global.get $g) (i32.const 2)))
        (global.get $g)
    )
)