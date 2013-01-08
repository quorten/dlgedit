# -*- gdb-script -*- definitions for debugging expandable arrays

define print_exparray
 set $i = 0
 while $i < $arg0.len
  print $arg0.d[$i++]
 end
end
document print_exparray
Print the contents of an expandable array.  Only one argument should
be given.
end

define print_garray
  set $i = 0
  while $i < $arg0->len
    print $arg0->data[$i++]
  end
end
document print_garray
Print the contents of a GArray.  Only one argument should be given.
end

define p_ea
 print_exparray $arg0
end
document p_ea
Print the contents of an expandable array.  Only one argument should
be given.
end

define p_ga
  print_garray $arg0
end
document p_ga
Print the contents of a GArray.  Only one argument should be given.
end

define p_sa
 print *$arg0.d@$arg0.len
end
document p_sa
Print the contents of an expandable array by using GDB's artificial
array support.  You should probably only use this function for
debugging short and simple arrays, hence the name "p_sa".
end

define p_sga
 print *$arg0->data@$arg0->len
end
document p_sa
Print the contents of a GArray by using GDB's artificial array
support.  You should probably only use this function for debugging
short and simple arrays, hence the name "p_sa".
end
