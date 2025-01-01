// SPDX-License-Identifier: GPL-2.0
/*
 * Simple kernel console driver for STM devices
 * Copyright (c) 2014, Intel Corporation.
 *
 * STM console will send kernel messages over STM devices to a trace host.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/console.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

static int csr_console_setup(struct console *c, char *opts);
static void csr_console_write(struct console *c, const char *buf, unsigned len);
static struct tty_driver *csr_console_device(struct console *c, int *index);


static struct console csr_console = {
  .name = "csr_console",
  .write		= csr_console_write,
  .setup		= csr_console_setup,
  .device               = csr_console_device,
  .flags		= CON_PRINTBUFFER | CON_CONSDEV ,
  .index		= -1,
};


struct ttyprintk_port {
	struct tty_port port;
	spinlock_t spinlock;
};

static struct ttyprintk_port tpk_port;

/*
 * Our simple preformatting supports transparent output of (time-stamped)
 * printk messages (also suitable for logging service):
 * - any cr is replaced by nl
 * - adds a ttyprintk source tag in front of each line
 * - too long message is fragmented, with '\'nl between fragments
 * - TPK_STR_SIZE isn't really the write_room limiting factor, because
 *   it is emptied on the fly during preformatting.
 */
#define TPK_STR_SIZE 508 /* should be bigger then max expected line length */
#define TPK_MAX_ROOM 4096 /* we could assume 4K for instance */

static void csr_print(const char *buf, int len) {
  int i;
  for(i = 0; i < len; i++) {
    while(csr_read(0xc03) != 0) {}
    csr_write(0xc03, buf[i]);
  }
}



/*
 * TTY operations open function.
 */
static int tpk_open(struct tty_struct *tty, struct file *filp)
{
	tty->driver_data = &tpk_port;
	return tty_port_open(&tpk_port.port, tty, filp);
}

/*
 * TTY operations close function.
 */
static void tpk_close(struct tty_struct *tty, struct file *filp)
{
	struct ttyprintk_port *tpkp = tty->driver_data;
	unsigned long flags;
	tty_port_close(&tpkp->port, tty, filp);
}

/*
 * TTY operations write function.
 */
static ssize_t tpk_write(struct tty_struct *tty,
			 const unsigned char *buf, size_t count)
{
	struct ttyprintk_port *tpkp = tty->driver_data;
	unsigned long flags;
	int ret;


	/* exclusive use of tpk_printk within this tty */
	spin_lock_irqsave(&tpkp->spinlock, flags);
	//ret = tpk_printk(buf, count);
	csr_print(buf, count);
	spin_unlock_irqrestore(&tpkp->spinlock, flags);

	return count;
}

/*
 * TTY operations write_room function.
 */
static unsigned int tpk_write_room(struct tty_struct *tty)
{
	return TPK_MAX_ROOM;
}

/*
 * TTY operations ioctl function.
 */
static int tpk_ioctl(struct tty_struct *tty,
			unsigned int cmd, unsigned long arg)
{
	struct ttyprintk_port *tpkp = tty->driver_data;

	if (!tpkp)
		return -EINVAL;

	switch (cmd) {
	/* Stop TIOCCONS */
	case TIOCCONS:
		return -EOPNOTSUPP;
	default:
		return -ENOIOCTLCMD;
	}
	return 0;
}

static const struct tty_operations ttyprintk_ops = {
	.open = tpk_open,
	.close = tpk_close,
	.write = tpk_write,
	.write_room = tpk_write_room,
	.ioctl = tpk_ioctl,
};

static const struct tty_port_operations null_ops = { };

static struct tty_driver *ttyprintk_driver;

static int __init ttyprintk_init(void)
{
	int ret;

	spin_lock_init(&tpk_port.spinlock);

	ttyprintk_driver = tty_alloc_driver(1,
			TTY_DRIVER_RESET_TERMIOS |
			TTY_DRIVER_REAL_RAW |
			TTY_DRIVER_UNNUMBERED_NODE);
	if (IS_ERR(ttyprintk_driver))
		return PTR_ERR(ttyprintk_driver);

	tty_port_init(&tpk_port.port);
	tpk_port.port.ops = &null_ops;

	ttyprintk_driver->driver_name = "ttyprintk";
	ttyprintk_driver->name = "ttyprintk";
	ttyprintk_driver->major = TTYAUX_MAJOR;
	ttyprintk_driver->minor_start = 3;
	ttyprintk_driver->type = TTY_DRIVER_TYPE_CONSOLE;
	ttyprintk_driver->init_termios = tty_std_termios;
	ttyprintk_driver->init_termios.c_oflag = OPOST | OCRNL | ONOCR | ONLRET;
	tty_set_operations(ttyprintk_driver, &ttyprintk_ops);
	tty_port_link_device(&tpk_port.port, ttyprintk_driver, 0);

	ret = tty_register_driver(ttyprintk_driver);
	if (ret < 0) {
		printk(KERN_ERR "Couldn't register ttyprintk driver\n");
		goto error;
	}

	return 0;

error:
	tty_driver_kref_put(ttyprintk_driver);
	tty_port_destroy(&tpk_port.port);
	return ret;
}

static void __exit ttyprintk_exit(void)
{
	tty_unregister_driver(ttyprintk_driver);
	tty_driver_kref_put(ttyprintk_driver);
	tty_port_destroy(&tpk_port.port);
}




static struct tty_driver *csr_console_device(struct console *c, int *index) {
  *index = c->index;
  return ttyprintk_driver;
}


static int csr_console_setup(struct console *c, char *opts) {
  ttyprintk_init();
  
  return 0;
}


static void csr_console_write(struct console *c, const char *buf, unsigned len) {
  csr_print(buf, len);
}

static void __exit csr_console_exit(void) {
  printk(KERN_INFO "HERE %s : %d\n", __PRETTY_FUNCTION__, __LINE__);  
  unregister_console(&csr_console);
}

static int __init csr_console_init(void) {
  printk(KERN_INFO "HERE %s : %d\n", __PRETTY_FUNCTION__, __LINE__);
  register_console(&csr_console);
  return 0;
}

module_init(csr_console_init);
module_exit(csr_console_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("csr_console driver");
MODULE_AUTHOR("David Sheffield");
