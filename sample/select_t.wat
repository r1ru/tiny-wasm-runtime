(module
    (func (export "select_t") (param i32)(result i32)
        (select (i32.const 1) (i32.const 2) (local.get 0))
    )
)