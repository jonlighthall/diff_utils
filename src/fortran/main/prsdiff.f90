program prsdiff
  ! prs diff - calculate difference between two pressure files
  !
  ! Aug 2022 JCL
  use file_diff_utils
  implicit none
  ! pressure
  real(kind=srk), dimension(:,:), allocatable :: pi1,pi2,pr1,pr2,tl1,tl2
  complex(kind=srk), dimension(:,:), allocatable :: p1,p2
  real(kind=srk):: dp,dp_max=0
  ! file parameters
  integer :: io,ls,n1,n2,np1,ns1,ns2,unit1,unit2
  character(len=256) :: dummy,fname1,fname2,tlthresh
  ! counters
  integer :: i,j
  integer :: nerr=0
  ! ----------------------------------------------------------
  ! set thresholds
  ! see ramio/outpt.f, format 20
  ! ranges are formatted as f12.2
  ! the minimum difference between ranges is therefore 0.01
  real(kind=srk),parameter :: rng_min_diff=0.01
  real(kind=srk) :: pdiff=1e-3
  ! ----------------------------------------------------------
  call get_command_argument(1,fname1,ln1)
  call get_command_argument(2,fname2,ln2)
  call get_command_argument(3,tlthresh,ln3)

  ! set file names
  if (ln1.eq.0) then
     fname1='nspe02.asc'
  end if
  if (ln2.eq.0) then
     fname2='std/case2r.rtl'
  end if

  if (ln3.gt.0) then
     read(tlthresh,*)pdiff
  end if

  ! open files
  open (newunit=unit1, file = fname1, status = 'old')
  open (newunit=unit2, file = fname2, status = 'old')

  ! check file length
  n1=0  ! number of lines
  ns1=0 ! number of spaces (delimiters)
  ls=0  ! position of last space
  do
     if (n1.eq.0) then
        read(unit1,'(a)',iostat=io) dummy
        do j=1,len(trim(dummy))
           if(dummy(j:j) == ' ') then
              if (j.eq.(ls+1)) then ! same space
              else                  ! new space
                 ns1=ns1+1
              endif
              ls =j
           endif
        enddo
     else
        read(unit1,*,iostat=io)
     endif
     if (io/=0) exit
     n1=n1+1
  enddo
  print '(1x,2a,i5,a,i3,a)',trim(fname1),' has ',n1,' lines and ',ns1,' delimiters'
  np1=(ns1)/2 ! number of complex pairs

  n2=0
  ns2=0
  ls=0
  do
     if (n2.eq.0) then
        read(unit2,'(a)',iostat=io) dummy
        do j=1,len(trim(dummy))
           if(dummy(j:j) == ' ') then
              if (j.eq.(ls+1)) then
              else
                 ns2=ns2+1
              endif
              ls =j
           endif
        enddo
     else
        read(unit2,*,iostat=io)
     endif
     if (io/=0) exit
     n2=n2+1
  enddo
102 format(1x,a,i5)
  if (n1.eq.n2) then
     print 102, 'file lengths match: ',n1
  else
     print *, 'file lengths do not match'
     print 102, 'length file 1 = ',n1
     print 102, 'length file 2 = ',n2
     close(unit1)
     close(unit2)
     stop 1
  endif

  if (ns1.eq.ns2) then
     print 102, 'number of file delimiters match: ',ns1
  else
     print *, 'number of file delimiters do not match'
     print 102, 'delim file 1 = ',ns1
     print 102, 'delim file 2 = ',ns2
     close(unit1)
     close(unit2)
     stop 1
  endif

  ! read file 1
  allocate(r1(n1),tl1(n1,ns1),p1(n1,np1),pr1(n1,np1),pi1(n1,np1))
  rewind(unit1)
  ! loop over lines
  do i = 1,n1
     read(unit1,*) r1(i), (tl1(i,j), j=1,ns1)
  end do
  close(unit1)
  ! read file 2 and compare ranges to file 1
  allocate(r2(n1),tl2(n1,ns1),p2(n1,np1),pr2(n1,np1),pi2(n1,np1))
  rewind(unit2)
  ! loop over lines
  do i = 1,n1
     read(unit2,*) r2(i), (tl2(i,j),j=1,ns1)
     if (abs(r1(i)-r2(i)).gt.rng_min_diff) then
        print *, 'ranges do not match'
        print *, 'range ',i,' file 1 = ',i,r1(i)
        print *, 'range ',i,' file 2 = ',i,r2(i)
        close(unit1)
        close(unit2)
        stop 1
     endif
  end do
  close(unit2)
  print *, 'ranges match'
  ! calculate complex pressure
  do i = 1,n1
     do j=1,np1
        pr1(i,j)=tl1(i,2*j-1)
        pi1(i,j)=tl1(i,2*j)
        p1(i,j)=cmplx(pr1(i,j),pi1(i,j))
     enddo
  enddo
  do i = 1,n1
     do j=1,np1
        pr2(i,j)=tl2(i,2*j-1)
        pi2(i,j)=tl2(i,2*j)
        p2(i,j)=cmplx(pr2(i,j),pi2(i,j))
     enddo
  enddo

  ! print summary
  ! loop over lines
  do i = 1,n1
     ! loop over pairs
     do j=1,np1
        ! calculate absolute difference
        dp=abs(p1(i,j)-p2(i,j))
        ! compare to maximum difference
        if (dp.gt.dp_max) dp_max=dp
        if(dp.gt.pdiff) then
           if (nerr.eq.0) then ! print table header on first error
              print'(/a)','   ix   iz    range    tl1    tl2 | diff'
              print*, repeat('-',33),'+',repeat('-',6)
           endif
           nerr=nerr+1
           write(*,'(2i5,f9.2, 4f9.5,a,f6.5)') i,j,r1(i), p1(i,j),p2(i,j),' | ',dp
        end if
     end do
  enddo
  print '(/a,f6.5,a,i0)',' number of errors found (>',pdiff,'): ',nerr
  print '(a,f6.4)',' maximum error : ',dp_max
end program prsdiff
