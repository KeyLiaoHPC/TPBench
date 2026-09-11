!---------------------------------------------------------------------
!---------------------------------------------------------------------
!
!  ep_data module
!
!---------------------------------------------------------------------
!---------------------------------------------------------------------
 
      module ep_data

!---------------------------------------------------------------------
!  The following include file is generated automatically by the
!  "setparams" utility, which defines the problem size 'm'
!---------------------------------------------------------------------

      include 'npbparams.h'

!---------------------------------------------------------------------
!   M is the Log_2 of the number of complex pairs of uniform (0, 1) random
!   numbers.  MK is the Log_2 of the size of each batch of uniform random
!   numbers.  MK can be set for convenience on a given system, since it does
!   not affect the results.
!---------------------------------------------------------------------
      integer    mk, mm, nn, nk, nq
      parameter (mk = 16, nk = 2 ** mk, nq = 10)

      double precision a, s
      parameter (a = 1220703125.d0, s = 271828183.d0)

! ... storage
      double precision x(2*nk), q(0:nq-1), qq(10000)

! ... timer constants
      integer    t_total, t_gpairs, t_randn, t_rcomm, t_last
      parameter (t_total=1, t_gpairs=2, t_randn=3, t_rcomm=4, t_last=4)

      end module ep_data


      subroutine tpb_npb_ep_set_class(cls, mval) bind(C, name="tpb_npb_ep_set_class")
      use iso_c_binding
      use ep_data
      implicit none
      character(c_char), value :: cls
      integer(c_int), value :: mval
      class = cls
      m = mval
      mm = m - mk
      nn = 2 ** mm
      return
      end
