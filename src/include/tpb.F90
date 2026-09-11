! tpb.F90
! ISO_C_BINDING interfaces for the typed TPBench kernel ABI.
module tpb
    use iso_c_binding
    implicit none
    private

    integer, parameter, public :: tpb_name_len = 256

    type, bind(C), public :: tpb_timer_t
        character(kind=c_char) :: name(256)
        integer(c_int64_t) :: unit
        integer(c_int64_t) :: dtype
        type(c_funptr) :: init
        type(c_funptr) :: tick
        type(c_funptr) :: tock
        type(c_funptr) :: get_stamp
    end type tpb_timer_t

    public :: tpb_k_get_timer
    public :: tpb_timer_tick
    public :: tpb_timer_tock
    public :: tpb_timer_elapsed_seconds
    public :: tpb_k_add_arg_i64_range
    public :: tpb_k_add_arg_i64_list
    public :: tpb_k_add_arg_char_list
    public :: tpb_k_get_arg_i64
    public :: tpb_k_get_arg_char
    public :: tpb_k_get_arg_double
    public :: tpb_k_register
    public :: tpb_k_add_output
    public :: tpb_k_alloc_output
    public :: tpb_k_finalize_pli

    interface
        function tpb_k_get_timer(timer) bind(C, name="tpb_k_get_timer")
            import :: c_int, tpb_timer_t
            integer(c_int) :: tpb_k_get_timer
            type(tpb_timer_t) :: timer
        end function tpb_k_get_timer

        function tpb_timer_tick(timer, ts) bind(C, name="tpb_timer_tick")
            import :: c_int, c_int64_t, tpb_timer_t
            integer(c_int) :: tpb_timer_tick
            type(tpb_timer_t) :: timer
            integer(c_int64_t) :: ts
        end function tpb_timer_tick

        function tpb_timer_tock(timer, ts) bind(C, name="tpb_timer_tock")
            import :: c_int, c_int64_t, tpb_timer_t
            integer(c_int) :: tpb_timer_tock
            type(tpb_timer_t) :: timer
            integer(c_int64_t) :: ts
        end function tpb_timer_tock

        function tpb_timer_elapsed_seconds(timer, elapsed, seconds) &
            bind(C, name="tpb_timer_elapsed_seconds")
            import :: c_int, c_int64_t, c_double, tpb_timer_t
            integer(c_int) :: tpb_timer_elapsed_seconds
            type(tpb_timer_t) :: timer
            integer(c_int64_t), value :: elapsed
            real(c_double) :: seconds
        end function tpb_timer_elapsed_seconds

        function tpb_k_add_arg_i64_range(name, tag, note, default_val, &
            lo, hi) bind(C, name="tpb_k_add_arg_i64_range")
            import :: c_int, c_int64_t, c_char
            integer(c_int) :: tpb_k_add_arg_i64_range
            character(kind=c_char) :: name(*)
            character(kind=c_char) :: tag(*)
            character(kind=c_char) :: note(*)
            character(kind=c_char) :: default_val(*)
            integer(c_int64_t), value :: lo
            integer(c_int64_t), value :: hi
        end function tpb_k_add_arg_i64_range

        function tpb_k_add_arg_i64_list(name, tag, note, default_val, &
            n, list) bind(C, name="tpb_k_add_arg_i64_list")
            import :: c_int, c_int64_t, c_char
            integer(c_int) :: tpb_k_add_arg_i64_list
            character(kind=c_char) :: name(*)
            character(kind=c_char) :: tag(*)
            character(kind=c_char) :: note(*)
            character(kind=c_char) :: default_val(*)
            integer(c_int), value :: n
            integer(c_int64_t) :: list(*)
        end function tpb_k_add_arg_i64_list

        function tpb_k_add_arg_char_list(name, tag, note, default_val, &
            n, list) bind(C, name="tpb_k_add_arg_char_list")
            import :: c_int, c_char
            integer(c_int) :: tpb_k_add_arg_char_list
            character(kind=c_char) :: name(*)
            character(kind=c_char) :: tag(*)
            character(kind=c_char) :: note(*)
            character(kind=c_char) :: default_val(*)
            integer(c_int), value :: n
            character(kind=c_char) :: list(*)
        end function tpb_k_add_arg_char_list

        function tpb_k_get_arg_i64(name, out) &
            bind(C, name="tpb_k_get_arg_i64")
            import :: c_int, c_int64_t, c_char
            integer(c_int) :: tpb_k_get_arg_i64
            character(kind=c_char) :: name(*)
            integer(c_int64_t) :: out
        end function tpb_k_get_arg_i64

        function tpb_k_get_arg_char(name, out) &
            bind(C, name="tpb_k_get_arg_char")
            import :: c_int, c_char
            integer(c_int) :: tpb_k_get_arg_char
            character(kind=c_char) :: name(*)
            character(kind=c_char) :: out
        end function tpb_k_get_arg_char

        function tpb_k_get_arg_double(name, out) &
            bind(C, name="tpb_k_get_arg_double")
            import :: c_int, c_double, c_char
            integer(c_int) :: tpb_k_get_arg_double
            character(kind=c_char) :: name(*)
            real(c_double) :: out
        end function tpb_k_get_arg_double

        function tpb_k_register(name, note, kctrl) &
            bind(C, name="tpb_k_register")
            import :: c_int, c_int32_t, c_char
            integer(c_int) :: tpb_k_register
            character(kind=c_char) :: name(*)
            character(kind=c_char) :: note(*)
            integer(c_int32_t), value :: kctrl
        end function tpb_k_register

        function tpb_k_add_output(name, tag, note, dtype, unit) &
            bind(C, name="tpb_k_add_output")
            import :: c_int, c_int64_t, c_char
            integer(c_int) :: tpb_k_add_output
            character(kind=c_char) :: name(*)
            character(kind=c_char) :: tag(*)
            character(kind=c_char) :: note(*)
            integer(c_int64_t), value :: dtype
            integer(c_int64_t), value :: unit
        end function tpb_k_add_output

        function tpb_k_alloc_output(name, n, ptr) &
            bind(C, name="tpb_k_alloc_output")
            import :: c_int, c_int64_t, c_ptr, c_char
            integer(c_int) :: tpb_k_alloc_output
            character(kind=c_char) :: name(*)
            integer(c_int64_t), value :: n
            type(c_ptr) :: ptr
        end function tpb_k_alloc_output

        function tpb_k_finalize_pli() bind(C, name="tpb_k_finalize_pli")
            import :: c_int
            integer(c_int) :: tpb_k_finalize_pli
        end function tpb_k_finalize_pli
    end interface
end module tpb
