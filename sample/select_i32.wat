(module
    (func (export "select_i32") (param i32)(result i32)
        (select (result i32) (i32.const 1) (i32.const 2) (local.get 0))
    )
)