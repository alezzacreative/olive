#include "renderthread.h"

#include <QApplication>
#include <QImage>
#include <QOpenGLFunctions>
#include <QDebug>

#include "ui/renderfunctions.h"
#include "playback/playback.h"
#include "project/sequence.h"

RenderThread::RenderThread() :
	frameBuffer(0),
	texColorBuffer(0),
	share_ctx(nullptr),
	ctx(nullptr),
	seq(nullptr),
	tex_width(-1),
	tex_height(-1),
	queued(false),
	texture_failed(false),
	running(true)
{
	surface.create();
}

RenderThread::~RenderThread() {
	surface.destroy();
}

void RenderThread::run() {
	mutex.lock();

	while (running) {
		if (!queued) {
			waitCond.wait(&mutex);
		}
		if (!running) {
			break;
		}
		queued = false;


		if (share_ctx != nullptr) {
			if (ctx != nullptr) {
				ctx->makeCurrent(&surface);

				// gen fbo
				if (frameBuffer == 0) {
					delete_fbo();
					ctx->functions()->glGenFramebuffers(1, &frameBuffer);
				}

				// bind
				ctx->functions()->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer);

				// gen texture
				if (texColorBuffer == 0 || tex_width != seq->width || tex_height != seq->height) {
					delete_texture();
					glGenTextures(1, &texColorBuffer);
					glBindTexture(GL_TEXTURE_2D, texColorBuffer);
					glTexImage2D(
						GL_TEXTURE_2D, 0, GL_RGB, seq->width, seq->height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr
					);
					tex_width = seq->width;
					tex_height = seq->height;
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					ctx->functions()->glFramebufferTexture2D(
						GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texColorBuffer, 0
					);
					glBindTexture(GL_TEXTURE_2D, 0);
				}

				// draw
				paint();

				// flush changes
				glFlush();

				// release
				ctx->functions()->glBindFramebuffer(GL_FRAMEBUFFER, 0);

				emit ready();
			}
		}
	}

	delete_ctx();

	mutex.unlock();
}

void RenderThread::paint() {
	glLoadIdentity();

	texture_failed = false;

	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	glClearColor(0, 0, 0, 0);
	glMatrixMode(GL_MODELVIEW);

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glEnable(GL_DEPTH);

	Effect* gizmos; // does nothing yet
	QVector<Clip*> nests;

	compose_sequence(nullptr, ctx, seq, nests, true, false, &gizmos, texture_failed);

	glDisable(GL_DEPTH);
	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
}

void RenderThread::start_render(QOpenGLContext *share, Sequence *s, int idivider) {
	if (s != seq && seq != nullptr) {
		closeActiveClips(seq);
	}

	seq = s;

	if (share != nullptr && (ctx == nullptr || ctx->shareContext() != share_ctx)) {
		share_ctx = share;
		delete_ctx();
		ctx = new QOpenGLContext();
		ctx->setFormat(share_ctx->format());
		ctx->setShareContext(share_ctx);
		ctx->create();
		ctx->moveToThread(this);
	}

	if (seq != nullptr) {
		queued = true;
		waitCond.wakeAll();
	}
}

bool RenderThread::did_texture_fail() {
	return texture_failed;
}

void RenderThread::cancel() {
	running = false;
	waitCond.wakeAll();
	wait();
}

void RenderThread::delete_texture() {
	if (texColorBuffer > 0) {
		ctx->functions()->glFramebufferTexture2D(
			GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0
		);
		glDeleteTextures(1, &texColorBuffer);
	}
	texColorBuffer = 0;
}

void RenderThread::delete_fbo() {
	if (frameBuffer > 0) {
		ctx->functions()->glDeleteFramebuffers(1, &frameBuffer);
	}
	frameBuffer = 0;
}

void RenderThread::delete_ctx() {
	if (ctx != nullptr) {
		delete_texture();
		delete_fbo();
		ctx->doneCurrent();
		delete ctx;
	}
	ctx = nullptr;
}
