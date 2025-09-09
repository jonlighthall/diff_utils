program tldiff
  ! TL diff - calculate difference between two transmission loss files
  !
  ! SYNTAX - tldiff FILE1_01.asc FILE2.tl DB_THRESH
  !
  ! ARGUMENTS
  !   FILE1 - test candidate, TL output: 01.asc, 02.asc, 03.asc
  !   FILE2 - STD reference,  TL output:   .tl,    .rtl,   .ftl
  !   DB_THRESH - acceptable error threshold in dB. Default value is 0.1dB, the
  !   minimum possible difference.
  !
  ! REQUIREMENTS - The files are assumed to be formatted with range in the first
  ! column and TL in the remaining columns.
  !
  ! METHOD - First, the dimensions of the two files are compared. The number of
  ! lines and the number of columns (delimiters) must match. Next, the ranges
  ! are compared (first column). Each range value must match to withing the
  ! parameter rdiff. Then, the TL is compared, element by element and a summary
  ! is printed.
  !
  ! FUNCTION - If any elements (less than the maximum TL value) differ by more
  ! than the sum of the parameters dtl_thresh and comp_diff, the program returns
  ! an error.
  !
  ! OUTPUT - four errors are tallied. Here, "error" refers to any difference
  ! between corresponding element values between the input two files. In the
  ! .asc files, TL is formatted as f6.1, therefor the minimum error that can be
  ! represented is 0.1dB. An error is defined as differences greater than
  ! 0.05dB. The remaining errors are based on the dB threshold specified in the
  ! command line argument. Errors greater than the dB threshold + 0.05dB are
  ! tallied. Any errors which exceed the limits of machine precision---that is,
  ! below 138dB---are ignored.
  !
  ! There are cases where small changes in TL occur at TL values less than
  ! 138dB, but that do not accumulate in range. That is, small differences occur
  ! near the TL nulls and disappear at lower TL values. To address these cases,
  ! a second threshold, called tl_max_user is employed. The hard-wired value of
  ! this threshold is 110dB. Errors in TL below this value will return an error.
  !
  !   NERR  - any errors > 0.05dB
  !   NERR2 - >dtl_error, TL any value
  !   NERR3 - >dtl_error, both TL less than hard-wired threshold 110dB
  !   NERR4 - >dtl_error, both TL less than epsilon threshold 138dB
  !
  ! Aug 2022 JCL
  use file_diff_utils
  implicit none
  ! TL
  real(kind=srk), dimension(:,:), allocatable :: tl1,tl2
  real(kind=srk)::dtl,dtl_max=0,dtl_max2=0,dtl_max3=0
  ! file parameters
  integer :: fstat,io,ls,n1,n2,ns1,ns2,unit1,unit2
  character(len=256) :: dummy,fname1,fname2,tlthresh
  logical :: is_error=.false.
  ! counters
  integer :: i,j
  integer :: nerr=0,nerr2=0,nerr3=0,nerr4=0
  ! ----------------------------------------------------------
  ! set thresholds
  ! see ramio/outpt.f, format 20
  ! ranges are formatted as f12.2
  ! the minimum difference between ranges is therefore 0.01
  real(kind=srk),parameter :: rng_min_diff=0.01
  ! TL is formatted as f6.1
  ! the minimum difference between ranges is therefore 0.1
  real(kind=srk),parameter :: tl_min_diff=0.1

  real(kind=srk) :: dtl_thresh=0.1
  real(kind=srk),parameter :: comp_diff=tl_min_diff/2.
  ! calculate the TL value corresponding to epsilon
  real(kind=srk),parameter :: tl_max_eps=-20*log10(2.**(-23))
  real(kind=srk),parameter :: tl_max_user=110
  real(kind=srk)::dtl_error
  ! ----------------------------------------------------------
  call get_command_argument(1,fname1,ln1)
  call get_command_argument(2,fname2,ln2)
  call get_command_argument(3,tlthresh,ln3)
100 format(a12,' = ',f7.3)
  print 100,'rng min dif',rng_min_diff
  print 100,'tl min dif',tl_min_diff
106 format(a12,a,f7.3,a)
  print 106,'tl max',' = '//achar(27)//'[34m',tl_max_eps,''//achar(27)//'[0m'

  ! set file names
  if (ln1.eq.0) then
     fname1='nspe02.asc'
  end if
  if (ln2.eq.0) then
     fname2='std/case2r.rtl'
  end if

  if (ln3.gt.0) then
     read(tlthresh,*)dtl_thresh
     print *, 'using user-defined dtl_thresh'
  else
     print *, 'using default dtl_thresh'
  end if

  print 106,'tl thresh',' = '//achar(27)//'[1;36m',dtl_thresh,''//achar(27)//'[0m'
  print 100,' tl diff',comp_diff
  dtl_error=dtl_thresh+comp_diff
  print 106,' tl error',' = '//achar(27)//'[31m',dtl_error,''//achar(27)//'[0m'

  if (tl_min_diff.gt.dtl_error) then
     print *, 'tl_min_diff < dtl_error'
  endif

  ! open files
open (newunit=unit1, file = fname1, status = 'old', iostat=fstat)
if (fstat /= 0) then
     print *, 'Error opening file: ', trim(fname1)
     stop 1
endif

open (newunit=unit2, file = fname2, status = 'old', iostat=fstat)
if (fstat /= 0) then
     print *, 'Error opening file: ', trim(fname2)
     stop 1
endif

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
     goto 88
  endif

  if (ns1.eq.ns2) then
     print 102, 'number of file delimiters match: ',ns1
  else
     print *, 'number of file delimiters do not match'
     print 102, 'delim file 1 = ',ns1
     print 102, 'delim file 2 = ',ns2
     goto 88
  endif

  ! read file 1
  allocate(r1(n1),tl1(n1,ns1))
  rewind(unit1)
  ! loop over lines
  do i = 1,n1
     read(unit1,*) r1(i), (tl1(i,j), j=1,ns1)
  end do
  close(unit1)
  ! read file 2 and compare ranges to file 1
  allocate(r2(n1),tl2(n1,ns1))
  rewind(unit2)
  ! loop over lines
  do i = 1,n1
     read(unit2,*) r2(i), (tl2(i,j),j=1,ns1)
     if (abs(r1(i)-r2(i)).gt.rng_min_diff) then
        print *, 'ranges do not match'
        print *, 'range ',i,' file 1 = ',i,r1(i)
        print *, 'range ',i,' file 2 = ',i,r2(i)
        goto 88
     endif
  end do
  close(unit2)
  print *, 'ranges match'

  ! print summary
  print *, 'tl1 = ',trim(fname1)
  print *, 'tl2 = ',trim(fname2)
101 format(a,f4.1,a)
  ! loop over lines
  do i = 1,n1
     ! loop over columns
     do j=1,ns1
        ! calculate absolute difference in TL
        dtl=abs(tl1(i,j)-tl2(i,j))
        ! compare to maximum difference in TL
        if (dtl.gt.dtl_max) dtl_max=dtl
        ! compare to tl_min_diff
        if(dtl.gt.comp_diff) then
           if (nerr.eq.0) then ! print table header on first error
              print'(/a)','   ix   iz    range    tl1    tl2 | diff'
              print*, repeat('-',33),'+',repeat('-',6)
           endif
           nerr=nerr+1
           write(*,'(2i5,f9.2)',advance='no') i,j,r1(i)

           ! check if TL1 is greater than maximum value
           if(tl1(i,j).gt.tl_max_eps)then
              write(*,'(a,f7.1,a)',advance='no') ''//achar(27)//'[34m',tl1(i,j),''//achar(27)//'[0m'
           elseif(tl1(i,j).gt.tl_max_user)then
              write(*,'(a,f7.1,a)',advance='no') ''//achar(27)//'[33m',tl1(i,j),''//achar(27)//'[0m'
           else
              write(*,'(f7.1)',advance='no') tl1(i,j)
           endif

           ! check if TL2 is greater than maximum value
           if(tl2(i,j).gt.tl_max_eps)then
              write(*,'(a,f7.1,a,a)',advance='no') ''//achar(27)//'[34m',tl2(i,j),''//achar(27)//'[0m',' | '
           elseif(tl2(i,j).gt.tl_max_user)then
              write(*,'(a,f7.1,a,a)',advance='no') ''//achar(27)//'[33m',tl2(i,j),''//achar(27)//'[0m',' | '
           else
              write(*,'(f7.1,a)',advance='no') tl2(i,j),' | '
           endif

           ! if both TL values are valid, check dTL against max
           ! check against maximum TL value
           if((tl1(i,j).lt.tl_max_eps).and.(tl2(i,j).lt.tl_max_eps))then
              if (dtl.gt.dtl_max2) dtl_max2=dtl
           endif
           ! check against user-specified value
           if((tl1(i,j).lt.tl_max_user).and.(tl2(i,j).lt.tl_max_user))then
              if (dtl.gt.dtl_max3) dtl_max3=dtl
           endif

           if(dtl.gt.(dtl_error)) then
              if((tl1(i,j).gt.tl_max_eps).or.(tl2(i,j).gt.tl_max_eps))then
                 print 101,''//achar(27)//'[34m',dtl,''//achar(27)//'[0m'
              elseif((tl1(i,j).gt.tl_max_user).or.(tl2(i,j).gt.tl_max_user))then
                 print 101,''//achar(27)//'[33m',dtl,''//achar(27)//'[0m'
              else
                 print 101,''//achar(27)//'[31m',dtl,''//achar(27)//'[0m'
              endif
              nerr2=nerr2+1
           else
              if (abs(dtl-dtl_thresh).lt.comp_diff) then
                 print 101,''//achar(27)//'[1;36m',dtl,''//achar(27)//'[0m'
              else
                 write(*,101) '',dtl,''
              endif
           endif

           if((tl1(i,j).lt.tl_max_user).and.(tl2(i,j).lt.tl_max_user).and.(dtl.gt.(dtl_error)))then
              nerr3=nerr3+1
           endif
           if((tl1(i,j).lt.tl_max_eps).and.(tl2(i,j).lt.tl_max_eps).and.(dtl.gt.(dtl_error)))then
              nerr4=nerr4+1
           endif

        endif
     enddo
  enddo
  print*
103 format(a,f6.3,a,i0)
  print 103,' number of errors found (>',tl_min_diff,'): ',nerr
  print 103,' number of errors found (>',dtl_error,'): ',nerr2
104 format (a,f6.3,a,f5.1,a,i0,a)
  if (nerr3.ne.nerr4) then
     print 104,' number of errors found (>',dtl_error,' and tl < ',tl_max_eps,'): '//achar(27)//'[33m'&
          &,nerr4,''//achar(27)//'[0m'
  endif
  if (nerr3.gt.0) then
     print 104,' number of errors found (>',dtl_error,' and tl < ',tl_max_user,'): '//achar(27)//'[31m'&
          &,nerr3,''//achar(27)//'[0m'
  else
     print 104,' number of errors found (>',dtl_error,' and tl < ',tl_max_user,'): '//achar(27)//'[32m'&
          &,nerr3,''//achar(27)//'[0m'
  endif
  print '(a,f6.3)',' maximum error : ',dtl_max
105 format(a,f5.1,a,f6.3)
  if(dtl_max-dtl_max3.gt.comp_diff) print 105,' maximum error (tl < ',tl_max_user,'): ',dtl_max3
  if(dtl_max-dtl_max2.gt.comp_diff) print 105,' maximum error (tl < ',tl_max_eps,'): ',dtl_max2
  if (abs(dtl_max-tl_min_diff).lt.comp_diff) then
     print *, ''//achar(27)//'[33mmax diff equals min diff'//achar(27)//'[0m'
  elseif (abs(dtl_max2-tl_min_diff).lt.comp_diff) then
     print 105,''//achar(27)//'[33m max diff (tl < ',tl_max_eps,') equals min diff'//achar(27)//'[0m'
  else
     if(dtl_max.lt.tl_min_diff) then
        print *, ''//achar(27)//'[32mmax diff less than min diff'//achar(27)//'[0m'
     endif
  endif

  print *, 'tl1 = ',trim(fname1)
  print *, 'tl2 = ',trim(fname2)
  if (nerr3.gt.0) then
     print '(a,f5.1,a,f6.3,a,f6.3)',''//achar(27)//'[31m max diff (tl < ',tl_max_user,') ',dtl_max3,' greater than ',dtl_error
     print *, ''//achar(27)//'[31mERROR'//achar(27)//'[0m'
     stop 1
  else
     print *, ''//achar(27)//'[32mOK'//achar(27)//'[0m'
  endif
  goto 77
88 is_error=.true.
  print*,'***ERROR***'
77 write(*,'(1x,a)',advance='no') 'closing files... '
  close(unit1,iostat=fstat,ERR=99)
  close(unit2,iostat=fstat,ERR=99)
  print*,'done'
  if (is_error) then
     print*,'stopping...'
     stop
  else
     goto 66
  endif
99 print*,'error closing file: ',fstat
66 print*,'goodbye'
end program tldiff
