module m8r

export File,
       init,
       input,
       output,
       histint,
       histfloat,
       histstring,
       getint,
       getfloat,
       getstring,
       getbool,
       gettype,
       leftsize,
       floatread,
       floatwrite,
       putint,
       putfloat,
       putstring,
       size,
       read

immutable File
    tag::String
    rsf::Ptr{UInt8}
end

function init()
    src = Base.source_path()
    argv = src == nothing ? ["julia"] : [basename(src)]
    if isempty(ARGS)
        push!(argv, "-")
    else
        append!(argv,ARGS)
    end
    ccall((:sf_init,"libdrsf"),Void,(Int32,Ptr{Ptr{UInt8}}),length(argv),argv)
end

function input(tag::String)
    rsf = ccall((:sf_input,"libdrsf"),Ptr{UInt8},(Ptr{UInt8},),tag)
    File(tag,rsf)
end

function output(tag::String)
    rsf = ccall((:sf_output,"libdrsf"),Ptr{UInt8},(Ptr{UInt8},),tag)
    File(tag,rsf)
end

function histint(file::File,name::String)
    val = Cint[0]
    ccall((:sf_histint,"libdrsf"),Bool,(Ptr{UInt8},Ptr{UInt8},Ptr{Cint}),file.rsf,name,val)
    return val[]
end

function histfloat(file::File,name::String)
    val = Cfloat[0]
    ccall((:sf_histfloat,"libdrsf"),Bool,(Ptr{UInt8},Ptr{UInt8},Ptr{Cfloat}),file.rsf,name,val)
    return val[]
end

function histstring(file::File,name::String)
    val = ccall((:sf_histstring,"libdrsf"),Ptr{Cchar},(Ptr{UInt8},Ptr{UInt8}),file.rsf,name)
    if val == C_NULL
        return ""
    end
    return unsafe_string(val)
end

function getint(name::String, val::Int)
    val = Cint[val]
    ccall((:sf_getint,"libdrsf"),Bool,(Ptr{UInt8},Ptr{Cint}),name,val)
    return val[]
end
getint(name; val::Int = 0) = getint(name, val)

function getfloat(name::String, val::Real)
    val = Cfloat[val]
    ccall((:sf_getfloat,"libdrsf"),Bool,(Ptr{UInt8},Ptr{Cfloat}),name,val)
    return val[]
end
getfloat(name::String; val::Real = 0) = getfloat(name, val)

function getstring(name::String, val::String)
    v = ccall((:sf_getstring,"libdrsf"),Ptr{Cchar},(Ptr{UInt8},),name)
    if v == C_NULL
        return val
    end
    return unsafe_string(v)
end
getstring(name::String; val::String = "") = getstring(name, val)

function getbool(name::String, val::Bool)
    val = Bool[val]
    ccall((:sf_getbool,"libdrsf"),Bool,(Ptr{UInt8},Ptr{Bool}),name,val)
    return val[]
end
getbool(name::String; val::Bool = true) = getbool(name, val)

function gettype(file::File)
    return ccall((:sf_gettype,"libdrsf"),Cuint,(Ptr{UInt8},),file.rsf) + 1
end

function leftsize(file::File,dim::Int)
    ccall((:sf_leftsize,"libdrsf"),Culonglong,(Ptr{UInt8},Cint),file.rsf,dim)
end

function floatread(arr::Array{Float32,1},size::Int32,file::File)
    ccall((:sf_floatread,"libdrsf"),Void,(Ptr{Cfloat},Csize_t,Ptr{UInt8}),arr,size,file.rsf)
end

function floatwrite(arr::Array{Float32,1},size::Int32,file::File)
    ccall((:sf_floatwrite,"libdrsf"),Void,(Ptr{Cfloat},Csize_t,Ptr{UInt8}),arr,size,file.rsf)
end

function putint(file::File,name::String,val::Int)
    val = convert(Cint, val)
    ccall((:sf_putint,"libdrsf"),Void,(Ptr{UInt8},Ptr{UInt8},Cint),file.rsf,name,val)
end

function putfloat(file::File,name::String,val::Real)
    val = convert(Cfloat, val)
    ccall((:sf_putfloat,"libdrsf"),Void,(Ptr{UInt8},Ptr{UInt8},Cfloat),file.rsf,name,val)
end

function putstring(file::File,name::String,val::String)
    ccall((:sf_putstring,"libdrsf"),Void,(Ptr{UInt8},Ptr{UInt8},Ptr{UInt8}),file.rsf,name,val)
end

function size(file::File)
    size = leftsize(file, 0)
    dim = 1
    s = Int32[]
    while size > 1
        n = histint(file, string("n", dim))
        push!(s, n)
        size /= n
        dim += 1
    end
    return s
end

function read(file::File)
    t = [UInt8, UInt8, Int, Float32, Complex64, Int16, Float64, Int]
    sh::Array{Int} = size(file)
    sz::Int32 = prod(sh)
    t_idx = gettype(file)
    data = zeros(t[t_idx], sz)
    if t_idx == 4
        floatread(data, sz, file)
    else
        throw("Can only read Float32 (not implemented)")
    end
    return reshape(data, sh...)
end

end
