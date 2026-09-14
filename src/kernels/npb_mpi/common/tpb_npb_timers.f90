! tpb_npb_timers.f90
! NPB Fortran timer ABI; clock is implemented in tpb_npb_timers.c.

      module timers
      implicit none
      end module timers

      subroutine timer_clear(n)
      implicit none
      integer n
      interface
         subroutine tpb_npb_timer_clear(n) bind(C, name="tpb_npb_timer_clear")
            use iso_c_binding
            integer(c_int), value :: n
         end subroutine tpb_npb_timer_clear
      end interface
      call tpb_npb_timer_clear(n)
      return
      end

      subroutine timer_start(n)
      implicit none
      integer n
      interface
         subroutine tpb_npb_timer_start(n) bind(C, name="tpb_npb_timer_start")
            use iso_c_binding
            integer(c_int), value :: n
         end subroutine tpb_npb_timer_start
      end interface
      call tpb_npb_timer_start(n)
      return
      end

      subroutine timer_stop(n)
      implicit none
      integer n
      interface
         subroutine tpb_npb_timer_stop(n) bind(C, name="tpb_npb_timer_stop")
            use iso_c_binding
            integer(c_int), value :: n
         end subroutine tpb_npb_timer_stop
      end interface
      call tpb_npb_timer_stop(n)
      return
      end

      double precision function timer_read(n)
      implicit none
      integer n
      interface
         function tpb_npb_timer_read(n) bind(C, name="tpb_npb_timer_read")
            use iso_c_binding
            real(c_double) :: tpb_npb_timer_read
            integer(c_int), value :: n
         end function tpb_npb_timer_read
      end interface
      timer_read = tpb_npb_timer_read(n)
      return
      end

      subroutine check_timer_flag(timeron)
      implicit none
      logical timeron
      interface
         function tpb_npb_check_timer_flag()  &
     &        bind(C, name="tpb_npb_check_timer_flag")
            use iso_c_binding
            integer(c_int) :: tpb_npb_check_timer_flag
         end function tpb_npb_check_timer_flag
      end interface
      timeron = (tpb_npb_check_timer_flag() .ne. 0)
      return
      end
