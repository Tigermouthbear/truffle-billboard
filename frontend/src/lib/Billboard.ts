interface BillboardApiMessage {
    type: string;
    admin?: boolean;
    config?: BillboardConfig;
    link?: string;
    timeout?: number;
    text?: string;
}

interface BillboardConfig {
    interval: number;
    groups: BillboardGroup[];
}

type BillboardGroup = BillboardItem[];

interface BillboardItem {
    type: string;
    link?: string;
    text?: string;
    image?: string;
    icon?: string;
    first?: string;
}

interface BillboardTheme {
    height: string;
    itemHeight: string;
    itemSpacing: string;
    itemInnerSpacing: string;
    borderRadius: string;

    lightBackground: string;
    darkBackground: string;
    lightHoverBackground: string;
    darkHoverBackground: string;
    lightTextColor: string;
    darkTextColor: string;
}

export type { BillboardApiMessage, BillboardConfig, BillboardGroup, BillboardItem, BillboardTheme }
