(module
    (memory 1)

    (func (export "as-store-value") (result i32)
        (i32.store (i32.const 5) (i32.const 0x10))
        (i32.load (i32.const 5))
    )
)