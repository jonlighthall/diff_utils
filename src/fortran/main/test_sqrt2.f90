program sqrt2_test
  implicit none
  real(kind=8) :: value
  integer :: i
  character(len=20) :: fmt
  
  value = sqrt(2.0d0)
  print '(a,f20.15)', 'sqrt(2) = ', value
  print '(a)', ''
  print '(a)', 'Different precisions:'
  
  do i = 0, 5
    if (i == 0) then
      print '(i0,1x,i0)', i, int(value)
    else
      write(fmt, '(a,i0,a,i0,a)') '(i0,1x,f', i+3, '.', i, ')'
      print fmt, i, value
    end if
  end do
end program sqrt2_test
