SECTIONS
{
    PROVIDE(__end__ = ADDR(.bss) + SIZEOF(.bss));
    PROVIDE(_end = ADDR(.bss) + SIZEOF(.bss));
    PROVIDE(end = ADDR(.bss) + SIZEOF(.bss));
}
