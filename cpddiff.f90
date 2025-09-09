program cpddiff
  ! cpd diff - calculate difference between two ducting probability files
  !
  ! Aug 2022 JCL
  use file_diff_utils
  implicit none
  ! TL
  real(kind=srk), dimension(:,:), allocatable :: tl1,tl2
  real(kind=srk)::dtl,dtl_max=0
  ! arguments
  integer :: ln1,ln2,ln3
  ! file parameters
  integer :: io,ls,n1,n2,ns1,ns2,unit1,unit2
  character(len=256) :: dummy,fname1,fname2,tlthresh
  ! counters
  integer :: i,j
  integer :: nerr=0,nerr2=0,nerr3=0
  ! ----------------------------------------------------------
  ! set thresholds
  ! see ramio/outpt.f, format 20
  ! ranges are formatted as f12.2
  ! the minimum difference between ranges is therefore 0.01
  real(kind=srk),parameter :: rng_min_diff=0.01
  real(kind=srk) :: tl_min_diff=0.001

  real(kind=srk) :: dtl_thresh=0.01
  real(kind=srk),parameter :: comp_diff=0.001
  real(kind=srk),parameter :: tlmax=-20*log10(2.**(-23))
  real(kind=srk)::dtl_error
  ! ----------------------------------------------------------
  integer :: nm1,nm2
  real(kind=srk)::f1,f2
  integer,dimension(:),allocatable :: dp1,dp2,dp_check
  real,dimension(:),allocatable :: p_check
  integer :: ld,pln,fln,dln,wln,aln
  character fmt*64
  call get_command_argument(1,fname1,ln1)
  call get_command_argument(2,fname2,ln2)
  call get_command_argument(3,tlthresh,ln3)
100 format(a12,' = ',f7.3)
  print 100,'rng min dif',rng_min_diff
  print 100,' tl max = ',tlmax
  print 100,' tl dif = ',dtl_thresh
  print 100,' tl com = ',comp_diff

  ! set file names
  if (ln1.eq.0) then
     fname1='calcpduct.out'
  end if
  if (ln2.eq.0) then
     fname2='std/std0_calcpduct.out'
  end if

  if (ln3.gt.0) then
     read(tlthresh,*)dtl_thresh
     print *, 'using user-defined dtl_thresh'
  else
     print *, 'using default dtl_thresh'
  end if

  print 100,' tl threh',dtl_thresh
  print 100,' tl diff',comp_diff
  dtl_error=dtl_thresh+comp_diff
  print 100,' tl error',dtl_error

  if (tl_min_diff.gt.dtl_error) then
     print *, 'tl_min_diff < dtl_error'
  endif

  ! open files
  open (newunit=unit1, file = fname1, status = 'old')
  open (newunit=unit2, file = fname2, status = 'old')

  ! check file length
  n1=0  ! number of lines
  ns1=0 ! number of spaces (delimiters)
  ls=0  ! position of last space
  do
     if (n1.eq.0) then
        read(unit1,*) nm1,f1 ! read header line
        print '(a)', 'header line:'
        print '(a,i0)',' nm1 = ',nm1
        print '(a,f7.1)','  f1 = ',f1
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
        ! check precision
        allocate(dp1(ns1+1))
        i=0
        ls=0
        do j=1,len(trim(dummy))
           if(dummy(j:j)=='.')ld=j
           if(dummy(j:j)==' ')then
              if(j.eq.(ls+1))then ! same space
              else                ! new space
                 i=i+1
                 dp1(i)=j-ld-1
              endif
              ls=j
           endif
        enddo
        dp1(ns1+1)=j-ld-1
        print*,dp1
     else
        read(unit1,*,iostat=io)
     endif
     if (io/=0) exit
     n1=n1+1
  enddo
  print '(1x,2a,i5,a,i3,a)',trim(fname1),' has ',n1,' lines and ',ns1,' delimiters'
  ! compare dimensions with header
  if (n1.eq.nm1) then
     print *, 'number of lines matches number of environments: ',n1
  else
     print *, 'number of lines does not match number of environments'
     print *, 'number of lines = ',n1
     print *, 'number of environments = ',nm1
     close(unit1)
     close(unit2)
     stop 'env'
  endif

  n2=0
  ns2=0
  ls=0
  do
     if (n2.eq.0) then
        read(unit2,*) nm2,f2 ! read header line
        print *,'nm2 = ',nm2
        print *,'f2 = ',f2
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
        ! check precision
        allocate(dp2(ns2+1))
        i=0
        ls=0
        do j=1,len(trim(dummy))
           if(dummy(j:j)=='.')ld=j
           if(dummy(j:j)==' ')then
              if(j.eq.(ls+1))then ! same space
              else                ! new space
                 i=i+1
                 dp2(i)=j-ld-1
              endif
              ls=j
           endif
        enddo
        dp2(ns2+1)=j-ld-1
        print*,dp2
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
  allocate(r1(n1),tl1(n1,ns1))
  rewind(unit1)
  read(unit1,*)dummy ! read header line
  ! loop over lines
  do i = 1,n1
     read(unit1,*) r1(i), (tl1(i,j), j=1,ns1)
  end do
  close(unit1)
  ! read file 2 and compare ranges to file 1
  allocate(r2(n1),tl2(n1,ns1))
  rewind(unit2)
  read(unit2,*)dummy ! read header line
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

  ! determine precision to check
  allocate(dp_check(ns1+1),p_check(ns1+1))
  do i=1,ns1+1
     dp_check(i)=min(dp1(i),dp2(i))
     p_check(i)=10.**(-dp_check(i))
     print'(es10.2)',p_check(i)
  enddo
  print*,'minimum precisions per column'
  print*,dp_check

  ! print summary
  ! loop over lines
  do i = 1,n1
     ! loop over columns
     do j=1,ns1
        ! calculate absolute difference in TL
        dtl=abs(tl1(i,j)-tl2(i,j))
        tl_min_diff=p_check(j+1)/2
        if(dtl-tl_min_diff.gt.epsilon(dtl)) then
                   if (dtl.gt.dtl_max) dtl_max=dtl
           if (nerr.eq.0) then ! print table header on first error
              print'(/a)','   ix   iz    range   tl1          tl2        |  diff        thresh'
              print*, repeat('-',45),'+',repeat('-',28)
           endif
           nerr=nerr+1
           write(*,'(2i5,f9.2)',advance='no') i,j,r1(i)
           pln=11
           wln=4
           dln=dp_check(j+1)+1
           aln=floor(log10(tl1(i,j)))
           fln=aln+1+dln+1
           write(fmt,'(a,i0,a,i0,a,i0,a,i0,a)')'(',wln-aln+1,'X,f',fln&
                &,'.',dln,',',max(0,pln-fln-(wln-aln))+1,'X)'
           write(*,fmt,advance='no') tl1(i,j)
           write(*,fmt,advance='no') tl2(i,j)
           write(*,'(a)',advance='no') ' | '
           write(*,fmt,advance='no') dtl

           write(*,fmt) tl_min_diff

           if((tl1(i,j).lt.tlmax).and.(tl2(i,j).lt.tlmax).and.(dtl.gt.(dtl_error)))then
              nerr3=nerr3+1
           endif
        endif
     enddo
  enddo
  print '(/a,f6.3,a,i0)',' number of errors found (>',tl_min_diff,'): ',nerr
  print '(a,f6.3,a,i0)',' number of errors found (>',dtl_error,'): ',nerr2
  print '(a,f6.3,a,f5.1,a,i0)',' number of errors found (>',dtl_error,' and tl < ',tlmax,'): ',nerr3
  print '(a,f8.5)',' maximum error : ',dtl_max

  print *, 'tl1 = ',trim(fname1)
  print *, 'tl2 = ',trim(fname2)
  if (nerr3.gt.0) then
     print *, ''//achar(27)//'[31mERROR'//achar(27)//'[0m'
     stop 1
  else
     print *, ''//achar(27)//'[32mOK'//achar(27)//'[0m'
  endif
end program cpddiff
