(module
  (type (;0;) (func (result i32)))
  (func (;0;) (type 0) (result i32)
    (local i32 i32)
    i32.const 0
    local.set 0
    i32.const 0
    local.set 1
    block  ;; label = @1
      loop  ;; label = @2
        local.get 1
        i32.const 10
        i32.lt_s
        i32.eqz
        br_if 1 (;@1;)
        local.get 0
        i32.const 2
        i32.div_s
        i32.const 2
        i32.mul
        local.get 0
        i32.ne
        if  ;; label = @3
          br 1 (;@2;)
        end
        local.get 0
        local.get 1
        i32.add
        local.set 0
        local.get 0
        drop
        local.get 1
        i32.const 1
        i32.add
        local.set 1
        local.get 1
        drop
        br 0 (;@2;)
      end
    end
    local.get 0
    return
    i32.const 0
    return)
  (export "main" (func 0)))
