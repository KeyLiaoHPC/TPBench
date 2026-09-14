! tpb_npb_result_f.f90
! Fortran wrapper for tpb_npb_set_result.

      subroutine tpb_npb_set_result_f(time_sec, mops, verified,  &
     &     niter, n1, n2, n3, cls)
      implicit none
      double precision time_sec, mops
      logical verified
      integer niter, n1, n2, n3
      character cls
      integer vflag
      interface
         subroutine tpb_npb_set_result(time_sec, mops, verified,  &
     &        niter, n1, n2, n3, cls) bind(C, name="tpb_npb_set_result")
            use iso_c_binding
            real(c_double), value :: time_sec, mops
            integer(c_int), value :: verified, niter, n1, n2, n3
            character(c_char), value :: cls
         end subroutine tpb_npb_set_result
      end interface
      vflag = 0
      if (verified) vflag = 1
      call tpb_npb_set_result(time_sec, mops, vflag, niter, n1, n2, n3,  &
     &     cls)
      return
      end

      subroutine tpb_npb_set_timer_vectors_f(nslot, t1, tming, tmaxg,  &
     &     tsum, nprocs_div)
      implicit none
      integer nslot, nprocs_div
      double precision t1(*), tming(*), tmaxg(*), tsum(*)
      interface
         subroutine tpb_npb_set_timer_vectors(nslot, t1, tming, tmaxg,  &
     &        tsum, nprocs_div) bind(C, name="tpb_npb_set_timer_vectors")
            use iso_c_binding
            integer(c_int), value :: nslot, nprocs_div
            real(c_double) :: t1(*), tming(*), tmaxg(*), tsum(*)
         end subroutine tpb_npb_set_timer_vectors
      end interface
      call tpb_npb_set_timer_vectors(nslot, t1, tming, tmaxg, tsum,  &
     &     nprocs_div)
      return
      end
